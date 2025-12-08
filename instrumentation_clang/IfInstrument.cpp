#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang;

namespace {

class IfInstrumentConsumer : public SemaConsumer {
public:
  void InitializeSema(Sema &S) override {
    TheSema = &S;
    TopLevelConsumer = &S.getASTConsumer();
    Ctx = &S.getASTContext();
    ensureRecordIfDecl();
  }

  void ForgetSema() override {
    TheSema = nullptr;
    TopLevelConsumer = nullptr;
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Ctx = &Context;
    emitHelperDecls();
  }

  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (Decl *D : DG) {
      auto *FD = dyn_cast<FunctionDecl>(D);
      if (!FD || FD == RecordIfDecl)
        continue;
      if (!FD->hasBody())
        continue;
      if (FD->isTemplated() || FD->isDependentContext())
        continue;

      Stmt *OldBody = FD->getBody();
      Stmt *NewBody = rewriteStmt(OldBody);
      if (NewBody && NewBody != OldBody)
        FD->setBody(NewBody);
    }

    return true;
  }

private:
  ASTContext *Ctx = nullptr;
  Sema *TheSema = nullptr;
  ASTConsumer *TopLevelConsumer = nullptr;
  FunctionDecl *RecordIfDecl = nullptr;
  llvm::SmallVector<Decl *, 1> PendingDecls;

  void ensureRecordIfDecl() {
    if (!Ctx || RecordIfDecl)
      return;

    TranslationUnitDecl *TU = Ctx->getTranslationUnitDecl();
    IdentifierInfo &Ident = Ctx->Idents.get("__record_if");
    QualType BoolTy = Ctx->BoolTy;

    QualType ParamTypes[] = {BoolTy};
    FunctionProtoType::ExtProtoInfo ProtoInfo;
    QualType FnType = Ctx->getFunctionType(BoolTy, ParamTypes, ProtoInfo);

    auto *FD =
        FunctionDecl::Create(*Ctx, TU, SourceLocation(), SourceLocation(),
                             &Ident, FnType,
                             Ctx->getTrivialTypeSourceInfo(FnType), SC_Extern);
    FD->setImplicit(true);

    auto *Param = ParmVarDecl::Create(
        *Ctx, FD, SourceLocation(), SourceLocation(),
        &Ctx->Idents.get("cond"), BoolTy,
        Ctx->getTrivialTypeSourceInfo(BoolTy), SC_None, nullptr);
    FD->setParams({Param});

    RecordIfDecl = FD;
    PendingDecls.push_back(FD);
  }

  void emitHelperDecls() {
    if (!Ctx || !TopLevelConsumer || PendingDecls.empty())
      return;

    for (Decl *D : PendingDecls) {
      Decl *Array[] = {D};
      DeclGroupRef DG = DeclGroupRef::Create(*Ctx, Array, 1);
      TopLevelConsumer->HandleTopLevelDecl(DG);
    }
    PendingDecls.clear();
  }

  Stmt *rewriteStmt(Stmt *S) {
    if (!S)
      return nullptr;

    if (auto *Compound = dyn_cast<CompoundStmt>(S))
      return rewriteCompoundStmt(Compound);
    if (auto *If = dyn_cast<IfStmt>(S))
      return rewriteIfStmt(If);
    if (auto *For = dyn_cast<ForStmt>(S)) {
      Stmt *Init = rewriteStmt(For->getInit());
      if (Init && Init != For->getInit())
        For->setInit(Init);
      Stmt *Body = rewriteStmt(For->getBody());
      if (Body && Body != For->getBody())
        For->setBody(Body);
      return For;
    }
    if (auto *Range = dyn_cast<CXXForRangeStmt>(S)) {
      Stmt *Init = rewriteStmt(Range->getInit());
      if (Init && Init != Range->getInit())
        Range->setInit(Init);
      Stmt *RangeStmt = rewriteStmt(Range->getRangeStmt());
      if (RangeStmt && RangeStmt != Range->getRangeStmt())
        Range->setRangeStmt(RangeStmt);
      Stmt *Begin = rewriteStmt(Range->getBeginStmt());
      if (Begin && Begin != Range->getBeginStmt())
        Range->setBeginStmt(Begin);
      Stmt *End = rewriteStmt(Range->getEndStmt());
      if (End && End != Range->getEndStmt())
        Range->setEndStmt(End);
      Stmt *LoopVar = rewriteStmt(Range->getLoopVarStmt());
      if (LoopVar && LoopVar != Range->getLoopVarStmt())
        Range->setLoopVarStmt(LoopVar);
      Stmt *Body = rewriteStmt(Range->getBody());
      if (Body && Body != Range->getBody())
        Range->setBody(Body);
      return Range;
    }
    if (auto *While = dyn_cast<WhileStmt>(S)) {
      Stmt *Body = rewriteStmt(While->getBody());
      if (Body && Body != While->getBody())
        While->setBody(Body);
      return While;
    }
    if (auto *Do = dyn_cast<DoStmt>(S)) {
      Stmt *Body = rewriteStmt(Do->getBody());
      if (Body && Body != Do->getBody())
        Do->setBody(Body);
      return Do;
    }
    if (auto *Switch = dyn_cast<SwitchStmt>(S)) {
      Stmt *Init = rewriteStmt(Switch->getInit());
      if (Init && Init != Switch->getInit())
        Switch->setInit(Init);
      Stmt *Body = rewriteStmt(Switch->getBody());
      if (Body && Body != Switch->getBody())
        Switch->setBody(Body);
      return Switch;
    }
    if (auto *Case = dyn_cast<CaseStmt>(S)) {
      Stmt *Sub = rewriteStmt(Case->getSubStmt());
      if (Sub && Sub != Case->getSubStmt())
        Case->setSubStmt(Sub);
      return Case;
    }
    if (auto *Default = dyn_cast<DefaultStmt>(S)) {
      Stmt *Sub = rewriteStmt(Default->getSubStmt());
      if (Sub && Sub != Default->getSubStmt())
        Default->setSubStmt(Sub);
      return Default;
    }
    if (auto *Label = dyn_cast<LabelStmt>(S)) {
      Stmt *Sub = rewriteStmt(Label->getSubStmt());
      if (Sub && Sub != Label->getSubStmt())
        Label->setSubStmt(Sub);
      return Label;
    }
    if (auto *Attr = dyn_cast<AttributedStmt>(S)) {
      Stmt *Sub = rewriteStmt(Attr->getSubStmt());
      if (!Sub)
        Sub = Attr->getSubStmt();
      return AttributedStmt::Create(*Ctx, Attr->getAttrLoc(), Attr->getAttrs(),
                                    Sub);
    }
    if (auto *Captured = dyn_cast<CapturedStmt>(S))
      return Captured;
    if (auto *CXXTry = dyn_cast<CXXTryStmt>(S)) {
      Stmt *TryBlock = rewriteStmt(CXXTry->getTryBlock());
      if (!TryBlock)
        TryBlock = CXXTry->getTryBlock();
      auto *TryCompound = cast<CompoundStmt>(TryBlock);

      llvm::SmallVector<Stmt *, 4> NewHandlers;
      NewHandlers.reserve(CXXTry->getNumHandlers());
      for (unsigned I = 0, E = CXXTry->getNumHandlers(); I != E; ++I) {
        CXXCatchStmt *Catch = CXXTry->getHandler(I);
        Stmt *Handler = rewriteStmt(Catch->getHandlerBlock());
        if (!Handler)
          Handler = Catch->getHandlerBlock();
        auto *NewCatch =
            new (*Ctx) CXXCatchStmt(Catch->getCatchLoc(),
                                    Catch->getExceptionDecl(), Handler);
        NewHandlers.push_back(NewCatch);
      }

      return CXXTryStmt::Create(*Ctx, CXXTry->getTryLoc(), TryCompound,
                                NewHandlers);
    }

    for (Stmt *Child : S->children())
      rewriteStmt(Child);
    return S;
  }

  Stmt *rewriteCompoundStmt(CompoundStmt *Compound) {
    llvm::SmallVector<Stmt *, 8> NewChildren;
    bool Changed = false;
    for (Stmt *Child : Compound->body()) {
      Stmt *NewChild = rewriteStmt(Child);
      if (NewChild != Child)
        Changed = true;
      NewChildren.push_back(NewChild);
    }

    if (!Changed)
      return Compound;

    return CompoundStmt::Create(*Ctx, NewChildren, Compound->getLBracLoc(),
                                Compound->getRBracLoc());
  }

  Stmt *rewriteIfStmt(IfStmt *If) {
    Stmt *Init = rewriteStmt(If->getInit());
    Stmt *Then = rewriteStmt(If->getThen());
    Stmt *Else = rewriteStmt(If->getElse());
    Expr *Cond = If->getCond();
    Expr *NewCond = Cond;

    if (shouldInstrument(*If))
      NewCond = buildRecordIfCall(Cond);

    return IfStmt::Create(*Ctx, If->getIfLoc(), If->getStatementKind(), Init,
                          If->getConditionVariable(), NewCond,
                          If->getLParenLoc(), If->getRParenLoc(), Then,
                          If->getElseLoc(), Else);
  }

  bool shouldInstrument(const IfStmt &If) const {
    if (!Ctx || !RecordIfDecl)
      return false;
    if (If.isConstexpr() || If.isConsteval())
      return false;

    const Expr *Cond = If.getCond();
    if (!Cond || Cond->isTypeDependent() || Cond->isValueDependent())
      return false;

    SourceLocation Loc = If.getIfLoc();
    if (Loc.isInvalid())
      return false;

    SourceManager &SM = Ctx->getSourceManager();
    if (SM.isInSystemHeader(Loc) || SM.isInSystemMacro(Loc))
      return false;

    return true;
  }

  Expr *buildRecordIfCall(Expr *Cond) const {
    if (!TheSema || !RecordIfDecl || !Cond)
      return Cond;

    DeclarationNameInfo NameInfo(RecordIfDecl->getDeclName(),
                                 Cond->getBeginLoc());
    DeclRefExpr *FuncRef = TheSema->BuildDeclRefExpr(
        RecordIfDecl, RecordIfDecl->getType(), VK_LValue, NameInfo);
    Expr *Arg = Cond;
    MultiExprArg Args(&Arg, 1);
    ExprResult CallRes = TheSema->BuildCallExpr(
        /*Scope=*/nullptr, FuncRef, Cond->getBeginLoc(), Args,
        Cond->getEndLoc());
    if (CallRes.isInvalid())
      return Cond;
    return CallRes.get();
  }
};

class IfInstrumentAction : public PluginASTAction {
protected:
  PluginASTAction::ActionType getActionType() override {
      return AddBeforeMainAction;
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<IfInstrumentConsumer>();
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<IfInstrumentAction>
    X("if-instrument", "Instrument if conditions with __record_if");
