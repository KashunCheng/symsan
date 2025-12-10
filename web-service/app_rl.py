"""
FastAPI Web Service for RL Driver - 新版本
提供 REST API 接口来调用 RL gRPC 后端进行分支路径追踪
"""
from fastapi import FastAPI, HTTPException, Request, Body
from fastapi.responses import FileResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi.exceptions import RequestValidationError
from starlette.exceptions import HTTPException as StarletteHTTPException
from pydantic import BaseModel, Field
import os
import uuid
import json
import threading
import traceback
import logging
from pathlib import Path
from typing import Optional, Dict, List
from datetime import datetime

from rl_driver_wrapper import run_rl_task, PROGRAM_ENDPOINTS, make_stub, get_all_lines

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="RL Driver Web Service",
    description="Web Service for RL-based symbolic execution with branch path tracing",
    version="2.0.0"
)

# 允许跨域（开发环境）
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 基准目录
BASE_DIR = Path(__file__).resolve().parent
RESULTS_DIR = Path(os.environ.get("RL_RESULTS_DIR", BASE_DIR / "results")).resolve()

# 创建必要的目录
RESULTS_DIR.mkdir(exist_ok=True)


# ==================== 请求/响应模型 ====================

class TraceRequest(BaseModel):
    """Trace 请求模型"""
    program: str = Field(..., description="程序名称: control_temp, dummy, complex")
    line_to_reach: int = Field(..., description="目标行的源码行号")
    branches: Dict[str, bool] = Field(
        default={},
        description="分支方向配置，格式为 {源码行号: 方向(true=taken/false=not_taken)}"
    )

    class Config:
        json_schema_extra = {
            "example": {
                "program": "control_temp",
                "line_to_reach": 85,
                "branches": {
                    "32": False,
                    "38": False,
                    "63": True,
                    "65": False
                }
            }
        }


class TraceResponse(BaseModel):
    """Trace 响应模型"""
    task_id: str
    status: str
    program: str
    line_to_reach: int
    message: str


# ==================== 异常处理器 ====================

@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    """捕获请求验证错误"""
    error_details = exc.errors()
    body = await request.body()
    logger.error(f"Validation error for {request.method} {request.url}")
    logger.error(f"Request body: {body[:1000] if body else 'empty'}")
    logger.error(f"Validation errors: {json.dumps(error_details, indent=2, default=str)}")
    return JSONResponse(
        status_code=422,
        content={
            "detail": error_details,
            "body": body.decode('utf-8', errors='replace')[:500] if body else None
        }
    )


@app.exception_handler(StarletteHTTPException)
async def http_exception_handler(request: Request, exc: StarletteHTTPException):
    """捕获 HTTP 异常"""
    logger.error(f"HTTP {exc.status_code} for {request.method} {request.url}: {exc.detail}")
    return JSONResponse(
        status_code=exc.status_code,
        content={"detail": exc.detail}
    )


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """捕获所有未处理的异常"""
    error_detail = traceback.format_exc()
    logger.error(f"Unhandled exception for {request.method} {request.url}:\n{error_detail}")
    return JSONResponse(
        status_code=500,
        content={
            "detail": str(exc),
            "type": type(exc).__name__,
            "traceback": error_detail
        }
    )


# ==================== API 端点 ====================

TRACE_REQUEST_EXAMPLE = {
    "program": "control_temp",
    "line_to_reach": 85,
    "branches": {
        "32": False,
        "38": False,
        "63": True,
        "65": False
    }
}


@app.post("/api/trace", response_model=TraceResponse)
async def submit_trace(
    req: TraceRequest = Body(..., examples=[TRACE_REQUEST_EXAMPLE])
):
    """
    提交一个分支路径追踪任务
    
    - **program**: 程序名称 (control_temp, dummy, complex)
    - **line_to_reach**: 目标行的源码行号
    - **branches**: 分支方向配置，key 是源码行号，value 是方向 (true=taken, false=not_taken)
    
    返回 task_id 用于查询状态和结果
    
    **示例请求:**
    ```json
    {
        "program": "control_temp",
        "line_to_reach": 85,
        "branches": {
            "32": false,
            "38": false,
            "63": true,
            "65": false
        }
    }
    ```
    """
    # 验证程序
    if req.program not in PROGRAM_ENDPOINTS:
        raise HTTPException(
            status_code=400,
            detail=f"Unknown program: {req.program}. Must be one of {list(PROGRAM_ENDPOINTS.keys())}"
        )
    
    task_id = str(uuid.uuid4())[:8]
    result_dir = RESULTS_DIR / task_id
    result_dir.mkdir(exist_ok=True)
    
    try:
        # 创建初始状态文件
        status = {
            "task_id": task_id,
            "status": "pending",
            "created_at": datetime.now().isoformat(),
            "updated_at": datetime.now().isoformat(),
            "program": req.program,
            "line_to_reach": req.line_to_reach,
            "branches": {str(k): v for k, v in req.branches.items()},
            "error": None,
            "result": None
        }
        status_path = result_dir / "status.json"
        with open(status_path, "w") as f:
            json.dump(status, f, indent=2)
        
        # 启动后台任务
        def task_runner():
            # 将字符串 key 转换为整数
            branches_int = {int(k): v for k, v in req.branches.items()}
            run_rl_task(
                task_id=task_id,
                program=req.program,
                line_to_reach=req.line_to_reach,
                branches=branches_int,
                result_dir=str(result_dir),
            )
        
        thread = threading.Thread(target=task_runner, daemon=True)
        thread.start()
        
        return TraceResponse(
            task_id=task_id,
            status="pending",
            program=req.program,
            line_to_reach=req.line_to_reach,
            message="Task submitted successfully"
        )
    
    except HTTPException:
        raise
    except Exception as e:
        error_detail = traceback.format_exc()
        logger.error(f"Failed to submit task: {error_detail}")
        raise HTTPException(
            status_code=500,
            detail=f"Failed to submit task: {type(e).__name__}: {str(e)}"
        )


@app.post("/api/trace/sync")
async def trace_sync(
    req: TraceRequest = Body(..., examples=[TRACE_REQUEST_EXAMPLE])
):
    """
    同步执行分支路径追踪（阻塞直到完成）
    
    适用于需要立即获取结果的场景
    
    **示例请求:**
    ```json
    {
        "program": "control_temp",
        "line_to_reach": 85,
        "branches": {
            "32": false,
            "38": false,
            "63": true,
            "65": false
        }
    }
    ```
    """
    # 验证程序
    if req.program not in PROGRAM_ENDPOINTS:
        raise HTTPException(
            status_code=400,
            detail=f"Unknown program: {req.program}. Must be one of {list(PROGRAM_ENDPOINTS.keys())}"
        )
    
    task_id = str(uuid.uuid4())[:8]
    result_dir = RESULTS_DIR / task_id
    result_dir.mkdir(exist_ok=True)
    
    try:
        # 将字符串 key 转换为整数
        branches_int = {int(k): v for k, v in req.branches.items()}
        
        # 直接执行任务（同步）
        run_rl_task(
            task_id=task_id,
            program=req.program,
            line_to_reach=req.line_to_reach,
            branches=branches_int,
            result_dir=str(result_dir),
        )
        
        # 读取结果
        rewards_path = result_dir / "rewards.json"
        if rewards_path.exists():
            with open(rewards_path, "r") as f:
                result = json.load(f)
            return result
        else:
            # 读取状态查看错误
            status_path = result_dir / "status.json"
            if status_path.exists():
                with open(status_path, "r") as f:
                    status = json.load(f)
                if status.get("error"):
                    raise HTTPException(status_code=500, detail=status["error"])
            raise HTTPException(status_code=500, detail="Task failed without result")
    
    except HTTPException:
        raise
    except Exception as e:
        error_detail = traceback.format_exc()
        logger.error(f"Trace sync failed: {error_detail}")
        raise HTTPException(
            status_code=500,
            detail=f"Trace failed: {type(e).__name__}: {str(e)}"
        )


@app.get("/api/status/{task_id}")
async def get_status(task_id: str):
    """
    查询任务状态
    """
    status_path = RESULTS_DIR / task_id / "status.json"
    
    if not status_path.exists():
        raise HTTPException(status_code=404, detail="Task not found")
    
    try:
        with open(status_path, "r") as f:
            status = json.load(f)
        
        # 如果任务已完成，尝试加载结果
        if status.get("status") == "completed":
            rewards_path = RESULTS_DIR / task_id / "rewards.json"
            if rewards_path.exists():
                with open(rewards_path, "r") as f:
                    status["result"] = json.load(f)
        
        return status
    
    except json.JSONDecodeError:
        raise HTTPException(status_code=500, detail="Invalid status file format")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to read status: {str(e)}")


@app.get("/api/download/{task_id}")
async def download_result(task_id: str):
    """
    下载任务结果文件 (rewards.json)
    """
    rewards_path = RESULTS_DIR / task_id / "rewards.json"
    
    if not rewards_path.exists():
        raise HTTPException(status_code=404, detail="Result file not found")
    
    return FileResponse(
        path=str(rewards_path),
        filename=f"rewards_{task_id}.json",
        media_type="application/json"
    )


@app.get("/api/lines/{program}")
async def get_lines(program: str):
    """
    获取程序的所有行信息
    
    用于查看可用的源码行号和 is_if 属性
    """
    if program not in PROGRAM_ENDPOINTS:
        raise HTTPException(
            status_code=400,
            detail=f"Unknown program: {program}. Must be one of {list(PROGRAM_ENDPOINTS.keys())}"
        )
    
    try:
        endpoint = PROGRAM_ENDPOINTS[program]
        stub, rl_pb2, channel = make_stub(endpoint)
        all_lines = get_all_lines(stub, rl_pb2)
        channel.close()
        
        # 转换为更友好的格式
        result = {}
        for lid, info in all_lines.items():
            result[info["line"]] = {
                "line_id": lid,
                "file": info["file"],
                "is_if": info["is_if"]
            }
        
        return {
            "program": program,
            "endpoint": endpoint,
            "lines": result
        }
    
    except Exception as e:
        raise HTTPException(
            status_code=500,
            detail=f"Failed to get lines: {type(e).__name__}: {str(e)}"
        )


@app.get("/api/programs")
async def list_programs():
    """
    列出所有可用的程序及其端点
    """
    return {
        "programs": PROGRAM_ENDPOINTS
    }


@app.get("/")
async def root():
    """
    根路径 - 返回 API 文档链接
    """
    return {
        "message": "RL Driver Web Service",
        "version": "2.0.0",
        "docs": "/docs",
        "api": {
            "trace": "POST /api/trace (异步追踪)",
            "trace_sync": "POST /api/trace/sync (同步追踪)",
            "status": "GET /api/status/{task_id}",
            "download": "GET /api/download/{task_id}",
            "lines": "GET /api/lines/{program}",
            "programs": "GET /api/programs",
        },
        "programs": list(PROGRAM_ENDPOINTS.keys()),
        "example": {
            "program": "control_temp",
            "line_to_reach": 85,
            "branches": {
                "32": False,
                "38": False,
                "63": True,
                "65": False
            }
        }
    }


@app.get("/health")
async def health():
    """健康检查端点"""
    # 检查各后端是否可用
    backend_status = {}
    for program, endpoint in PROGRAM_ENDPOINTS.items():
        try:
            stub, rl_pb2, channel = make_stub(endpoint)
            # 尝试调用 ReadLines
            get_all_lines(stub, rl_pb2)
            channel.close()
            backend_status[program] = "healthy"
        except Exception as e:
            backend_status[program] = f"unhealthy: {str(e)}"
    
    all_healthy = all(s == "healthy" for s in backend_status.values())
    
    return {
        "status": "healthy" if all_healthy else "degraded",
        "backends": backend_status
    }
