#include "defs.h"
#include "debug.h"
#include "version.h"

#include "dfsan/dfsan.h"

extern "C" {
#include "launch.h"
}

#include "parse-z3.h"

#include "rl.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <msgpack.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <dirent.h>
#include <limits.h>
#include <experimental/optional>

using std::experimental::optional;
using std::experimental::nullopt;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using namespace __dfsan;

namespace {

struct LineInfo {
  std::string file;
  uint32_t line;
  bool is_if;
};

struct BranchInfo {
  uint64_t line_id = 0;
  uint64_t cid = 0;
  bool is_if = false;
  bool seen = false;
  bool symbolic = false;
  bool taken = false;
  bool not_taken = false;
  bool last_result = false;
};

struct Config {
  std::string target;
  std::vector<std::string> args;
  std::string input_path;
  std::string output_dir = ".";
  std::string workdir = ".";
  std::string listen_addr = "0.0.0.0:50051";
  bool use_stdin = false;
  bool debug = false;
  bool bounds_check = false;
  bool solve_ub = false;
  unsigned run_timeout_ms = 5000;
  unsigned solve_timeout_ms = 5000;
  unsigned max_runs = 8;
};

struct CondEntry {
  uint32_t label = 0;
  uint8_t result = 0;
};

struct RunResult {
  bool timed_out = false;
  std::vector<std::string> new_inputs;
  std::vector<uint8_t> input_bytes;
  std::string input_path;
  std::unordered_map<uint64_t, BranchInfo> branches;
  std::unordered_set<uint64_t> line_hits;
};

class RLDriver {
public:
  explicit RLDriver(Config cfg)
      : cfg_(std::move(cfg)), z3_ctx_(),
        parser_(nullptr),
        union_base_(nullptr),
        next_input_id_(0) {}
  ~RLDriver() { symsan_destroy(); }

  bool init();
  bool load_linecov();
  Status HandleTrace(const rl::TraceRequest &req, rl::TraceResponse *resp);
  Status HandleReadLines(rl::ReadLinesResponse *resp);
  void Serve();

private:
  bool run_once(const std::string &input_path, bool report_all, RunResult &res);
  bool mmap_input(const std::string &path, std::vector<uint8_t> &buf, int &fd);
  std::string materialize_input(const std::vector<uint8_t> &buf);
  bool branches_covered(const std::unordered_map<uint64_t, bool> &targets);
  bool run_has_branches(const RunResult &run,
                        const std::unordered_map<uint64_t, bool> &targets) const;
  bool ensure_run_has_branches(const std::unordered_map<uint64_t, bool> &targets,
                               RunResult &run);
  bool explore_until_covered(const std::unordered_map<uint64_t, bool> &targets,
                             RunResult &last_run);
  bool build_branch_maps(uint64_t line_id,
                         std::unordered_map<uint64_t, bool> &sym,
                         std::unordered_map<uint64_t, bool> &non_sym,
                         const std::unordered_map<uint64_t, BranchInfo> &source);
  optional<RunResult> solve_for_trace(const std::unordered_map<uint64_t, bool> &branch_trace,
                                      const std::vector<uint8_t> &buf);

  Config cfg_;
  z3::context z3_ctx_;
  std::unique_ptr<symsan::Z3ParserSolver> parser_;
  void *union_base_;
  std::unordered_map<uint64_t, LineInfo> line_table_;
  std::unordered_map<uint64_t, BranchInfo> branches_;
  std::unordered_map<uint64_t, uint64_t> line_to_cid_;
  std::unordered_map<uint64_t, CondEntry> conds_; // cid -> label/result
  std::unordered_map<uint64_t, std::string> branch_inputs_;
  std::deque<std::string> input_queue_;
  std::string last_input_path_;
  uint32_t session_id_ = 0;
  uint32_t next_input_id_;
  std::mutex mu_;
};

static bool has_linecov_suffix(const char *name) {
  static const char suffix[] = ".linecov.msgpack";
  size_t len = strlen(name);
  size_t suffix_len = sizeof(suffix) - 1;
  if (len < suffix_len)
    return false;
  return strcmp(name + len - suffix_len, suffix) == 0;
}

static std::string make_linecov_path(const std::string &dir, const char *name) {
  if (dir.empty() || dir == ".")
    return std::string(name);
  if (dir.back() == '/')
    return dir + name;
  return dir + "/" + name;
}

bool RLDriver::load_linecov() {
  std::vector<std::string> dirs;
  dirs.emplace_back(".");
  if (!cfg_.target.empty()) {
    auto pos = cfg_.target.find_last_of('/');
    if (pos != std::string::npos) {
      std::string dir = cfg_.target.substr(0, pos);
      if (dir.empty())
        dir = "/";
      dirs.emplace_back(dir);
    }
  }

  std::unordered_set<std::string> visited;
  for (const auto &dir : dirs) {
    if (dir.empty())
      continue;
    if (!visited.insert(dir).second)
      continue;
    DIR *d = opendir(dir.c_str());
    if (!d)
      continue;
    struct dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
      if (ent->d_name[0] == '.')
        continue;
      if (!has_linecov_suffix(ent->d_name))
        continue;
      std::string path = make_linecov_path(dir, ent->d_name);
      std::ifstream input(path, std::ios::binary | std::ios::ate);
      if (!input.is_open())
        continue;
      std::streamsize size = input.tellg();
      if (size <= 0)
        continue;
      input.seekg(0, std::ios::beg);
      std::vector<char> buffer(static_cast<size_t>(size));
      if (!input.read(buffer.data(), size))
        continue;
      try {
        msgpack::object_handle handle =
            msgpack::unpack(buffer.data(), buffer.size());
        msgpack::object obj = handle.get();
        if (obj.type != msgpack::type::MAP)
          continue;
        auto *pairs = obj.via.map.ptr;
        for (uint32_t i = 0; i < obj.via.map.size; ++i) {
          std::string key_str;
          pairs[i].key.convert(key_str);
          uint64_t line_id = std::stoull(key_str);
          msgpack::object &val = pairs[i].val;
          if (val.type != msgpack::type::ARRAY || val.via.array.size < 3)
            continue;
          std::string file;
          val.via.array.ptr[0].convert(file);
          uint32_t line = 0;
          val.via.array.ptr[1].convert(line);
          bool is_if = false;
          val.via.array.ptr[2].convert(is_if);
          line_table_[line_id] = {std::move(file), line, is_if};
        }
        fprintf(stderr, "loaded %u line coverage entries from %s\n",
                obj.via.map.size, path.c_str());
      } catch (const std::exception &e) {
        fprintf(stderr, "Failed to parse %s: %s\n", path.c_str(), e.what());
      }
    }
    closedir(d);
  }
  return !line_table_.empty();
}

bool RLDriver::init() {
  if (cfg_.target.empty() || cfg_.input_path.empty()) {
    fprintf(stderr, "Invalid config: target or input missing\n");
    return false;
  }
  input_queue_.push_back(cfg_.input_path);
  return true;
}

bool RLDriver::mmap_input(const std::string &path, std::vector<uint8_t> &buf,
                          int &fd) {
  struct stat st {};
  fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Failed to open %s: %s\n", path.c_str(), strerror(errno));
    return false;
  }
  if (fstat(fd, &st) != 0) {
    fprintf(stderr, "Failed to stat %s: %s\n", path.c_str(), strerror(errno));
    close(fd);
    return false;
  }
  if (st.st_size == 0) {
    buf.clear();
    return true;
  }
  void *p = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ,
                 MAP_PRIVATE, fd, 0);
  if (p == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap %s: %s\n", path.c_str(), strerror(errno));
    close(fd);
    return false;
  }
  buf.assign(reinterpret_cast<uint8_t *>(p),
             reinterpret_cast<uint8_t *>(p) + st.st_size);
  munmap(p, static_cast<size_t>(st.st_size));
  return true;
}

std::string RLDriver::materialize_input(const std::vector<uint8_t> &buf) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/id-%u-%u", cfg_.output_dir.c_str(),
           session_id_, next_input_id_++);
  int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    fprintf(stderr, "Failed to create %s: %s\n", path, strerror(errno));
    return {};
  }
  if (!buf.empty()) {
    ssize_t w = write(fd, buf.data(), buf.size());
    if (w < 0 || static_cast<size_t>(w) != buf.size()) {
      fprintf(stderr, "Failed to write %s: %s\n", path, strerror(errno));
      close(fd);
      return {};
    }
  }
  close(fd);
  return std::string(path);
}

bool RLDriver::run_once(const std::string &input_path, bool report_all,
                        RunResult &res) {
  res.timed_out = false;
  res.input_path = input_path;
  res.branches.clear();
  res.line_hits.clear();
  int input_fd = -1;
  if (!mmap_input(input_path, res.input_bytes, input_fd)) {
    spdlog::error("Failed to mmap input {}", input_path);
    return false;
  }

  if (!union_base_) {
    union_base_ = symsan_init(cfg_.target.c_str(), uniontable_size);
    if (union_base_ == (void *)-1) {
      fprintf(stderr, "symsan_init failed: %s\n", strerror(errno));
      spdlog::error("symsan_init failed: {}", strerror(errno));
      return false;
    }
  }
  // recreate parser each run to ensure a fresh solver/context per request
  parser_.reset(new symsan::Z3ParserSolver(union_base_, uniontable_size,
                                           z3_ctx_));
  if (!parser_) {
    spdlog::error("Failed to create parser");
    return false;
  }

  std::vector<std::string> run_args = cfg_.args;
  if (run_args.empty()) {
    run_args.push_back(cfg_.target);
    run_args.push_back(input_path);
  } else {
    for (auto &a : run_args) {
      size_t pos = a.find("{input}");
      if (pos != std::string::npos) {
        a.replace(pos, 7, input_path);
      }
    }
  }

  std::vector<char *> argv;
  for (auto &a : run_args)
    argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(nullptr);

  symsan_set_input(cfg_.use_stdin ? "stdin" : input_path.c_str());
  if (symsan_set_input(cfg_.use_stdin ? "stdin" : input_path.c_str()) != 0) {
    spdlog::error("symsan_set_input failed for {}", cfg_.use_stdin ? "stdin" : input_path);
    close(input_fd);
    return false;
  }
  if (symsan_set_args(static_cast<int>(run_args.size()), argv.data()) != 0) {
    spdlog::error("symsan_set_args failed");
    close(input_fd);
    return false;
  }
  symsan_set_debug(cfg_.debug);
  symsan_set_bounds_check(cfg_.bounds_check);
  symsan_set_solve_ub(cfg_.solve_ub);
  symsan_set_force_stdin(cfg_.use_stdin);
  symsan_set_always_report_coverage(report_all);

  spdlog::trace("ready to execute symsan_run.");
  int ret = symsan_run(input_fd);
  spdlog::trace("symsan_run returns.");
  if (ret != 0) {
    fprintf(stderr, "symsan_run failed %d\n", ret);
    spdlog::error("symsan_run failed {}", ret);
    close(input_fd);
    return false;
  }

  std::vector<symsan::input_t> inputs;
  inputs.push_back({res.input_bytes.data(), res.input_bytes.size()});
  parser_->restart(inputs);

  pipe_msg msg {};
  gep_msg gmsg {};
  memcmp_msg *mmsg = nullptr;
  std::vector<uint64_t> tasks;
  size_t msg_size = 0;
  bool new_cov_since_task = true; // allow initial task creation per run
  spdlog::trace("ready to receive the first message.");
  while (symsan_read_event(&msg, sizeof(msg), cfg_.run_timeout_ms) ==
         sizeof(msg)) {
    switch (msg.msg_type) {
    case cond_type: {
      conds_[msg.id] = {msg.label, static_cast<uint8_t>(msg.result)};
      if (!new_cov_since_task) {
        break;
      }
      size_t before = tasks.size();
      parser_->parse_cond(msg.label, msg.result, msg.flags & F_ADD_CONS, tasks);
      if (tasks.size() > before) {
        new_cov_since_task = false;
        spdlog::debug("Added cond task for cid={} label={}", msg.id, msg.label);
      }
      break;
    }
    case gep_type: {
      if (!new_cov_since_task) {
        // still need to drain the gep payload to keep stream in sync
        if (symsan_read_event(&gmsg, sizeof(gmsg), 0) != sizeof(gmsg)) {
          fprintf(stderr, "Failed to read gep msg: %s\n", strerror(errno));
        }
        break;
      }
      if (symsan_read_event(&gmsg, sizeof(gmsg), 0) != sizeof(gmsg)) {
        fprintf(stderr, "Failed to read gep msg: %s\n", strerror(errno));
        break;
      }
      size_t before = tasks.size();
      parser_->parse_gep(gmsg.ptr_label, gmsg.ptr, gmsg.index_label, gmsg.index,
                         gmsg.num_elems, gmsg.elem_size, gmsg.current_offset,
                         true, tasks);
      if (tasks.size() > before) {
        new_cov_since_task = false;
        spdlog::debug("Added gep task for label={}", gmsg.index_label);
      }
      break;
    }
    case memcmp_type: {
      if (msg.label == 0)
        break;
      msg_size = sizeof(memcmp_msg) + msg.result;
      mmsg = (memcmp_msg *)malloc(msg_size);
      if (!mmsg)
        break;
      if (symsan_read_event(mmsg, msg_size, 0) != (ssize_t)msg_size) {
        fprintf(stderr, "Failed to read memcmp msg: %s\n", strerror(errno));
        free(mmsg);
        break;
      }
      parser_->record_memcmp(msg.label, mmsg->content, msg.result);
      free(mmsg);
      break;
    }
    case cover_type: {
      uint64_t line_id =
          (static_cast<uint64_t>(msg.context) << 32) | msg.label;
      auto lt_it = line_table_.find(line_id);
      bool is_if = (lt_it != line_table_.end()) ? lt_it->second.is_if : false;

      // branch bookkeeping (only if it is a branch)
      if (is_if || msg.id != 0) {
        BranchInfo &bi = branches_[line_id];
        bi.line_id = line_id;
        bi.cid = msg.id;
        bi.is_if = is_if;
        bi.seen = true;
        bi.last_result = msg.result;
        if (msg.result)
          bi.taken = true;
        else
          bi.not_taken = true;
        if (msg.flags & F_BRANCH_TAKEN_HISTORY)
          bi.taken = true;
        if (msg.flags & F_BRANCH_NOT_TAKEN_HISTORY)
          bi.not_taken = true;
        if (msg.flags & F_BRANCH_SYMBOLIC)
          bi.symbolic = true;
        line_to_cid_[line_id] = msg.id;

        // per-run record
        BranchInfo &rbi = res.branches[line_id];
        rbi = bi;
        branch_inputs_[line_id] = res.input_path;
      }

      // runtime only reports new coverage unless we explicitly ask for all hits
      if (!report_all) {
        new_cov_since_task = true;
        spdlog::debug("New coverage line_id={} cid={}", line_id, msg.id);
      }
      res.line_hits.insert(line_id);
      break;
    }
    default:
      break;
    }
  }

  if (errno == EINTR) {
    res.timed_out = true;
  }

  close(input_fd);

  for (auto tid : tasks) {
    symsan::Z3ParserSolver::solution_t solutions;
    auto st = parser_->solve_task(tid, cfg_.solve_timeout_ms, solutions);
    if (st == symsan::Z3ParserSolver::opt_sat ||
        st == symsan::Z3ParserSolver::nested_sat ||
        st == symsan::Z3ParserSolver::opt_sat_nested_timeout ||
        st == symsan::Z3ParserSolver::opt_sat_nested_unsat) {
      if (!solutions.empty()) {
        std::vector<uint8_t> new_buf = res.input_bytes;
        for (auto const &sol : solutions) {
          if (sol.offset < new_buf.size()) {
            new_buf[sol.offset] = sol.val;
          }
        }
        std::string np = materialize_input(new_buf);
        if (!np.empty())
          res.new_inputs.push_back(np);
      }
    }
  }

  last_input_path_ = input_path;
  return true;
}

bool RLDriver::branches_covered(const std::unordered_map<uint64_t, bool> &targets) {
  for (auto const &kv : targets) {
    auto it = branches_.find(kv.first);
    if (it == branches_.end() || !it->second.seen)
      return false;
  }
  return true;
}

bool RLDriver::run_has_branches(
    const RunResult &run, const std::unordered_map<uint64_t, bool> &targets) const {
  if (targets.empty())
    return true;
  for (auto const &kv : targets) {
    auto it = run.branches.find(kv.first);
    if (it == run.branches.end() || !it->second.seen)
      return false;
  }
  return true;
}

bool RLDriver::ensure_run_has_branches(
    const std::unordered_map<uint64_t, bool> &targets, RunResult &run) {
  if (run_has_branches(run, targets))
    return true;
  std::vector<std::string> candidates;
  for (auto const &kv : targets) {
    auto bi = branch_inputs_.find(kv.first);
    if (bi != branch_inputs_.end() && !bi->second.empty())
      candidates.push_back(bi->second);
  }
  if (!last_input_path_.empty())
    candidates.push_back(last_input_path_);
  if (!cfg_.input_path.empty())
    candidates.push_back(cfg_.input_path);

  std::vector<std::string> unique;
  std::unordered_set<std::string> seen;
  for (auto const &path : candidates) {
    if (path.empty())
      continue;
    if (seen.insert(path).second)
      unique.push_back(path);
  }

  for (auto const &path : unique) {
    RunResult candidate;
    spdlog::debug("Replaying input {} to refresh branch coverage", path);
    if (!run_once(path, true, candidate)) {
      spdlog::warn("Failed to replay {}", path);
      continue;
    }
    if (run_has_branches(candidate, targets)) {
      run = std::move(candidate);
      return true;
    }
  }
  return false;
}

bool RLDriver::explore_until_covered(
    const std::unordered_map<uint64_t, bool> &targets, RunResult &last_run) {
  unsigned runs = 0;
  while (!branches_covered(targets) && !input_queue_.empty() &&
         runs < cfg_.max_runs) {
    std::string ip = input_queue_.front();
    input_queue_.pop_front();
    RunResult res;
    if (!run_once(ip, false, res))
      return false;
    for (auto const &np : res.new_inputs)
      input_queue_.push_back(np);
    last_run = std::move(res);
    runs++;
  }
  return branches_covered(targets);
}

bool RLDriver::build_branch_maps(
    uint64_t line_id, std::unordered_map<uint64_t, bool> &sym,
    std::unordered_map<uint64_t, bool> &non_sym,
    const std::unordered_map<uint64_t, BranchInfo> &source) {
  auto it = source.find(line_id);
  if (it == source.end() || !it->second.seen)
    return false;
  if (it->second.symbolic) {
    sym[line_id] = it->second.last_result;
  } else {
    non_sym[line_id] = it->second.last_result;
  }
  return true;
}

optional<RunResult> RLDriver::solve_for_trace(const std::unordered_map<uint64_t, bool> &branch_trace,
                                              const std::vector<uint8_t> &input_bytes) {
  std::vector<symsan::input_t> inputs;
  inputs.push_back({input_bytes.data(), input_bytes.size()});
  parser_->restart(inputs);
  // capture cond metadata for symbolic branches once so we can reuse below
  std::unordered_map<uint64_t, CondEntry> symbolic_meta;
  uint64_t task_id = std::numeric_limits<uint64_t>::max();
  for (auto const &kv : branch_trace) {
    auto bit = branches_.find(kv.first);
    if (bit == branches_.end()) {
      spdlog::warn("Requested branch {} not observed yet", kv.first);
      return nullopt;
    }
    if (!bit->second.symbolic)
      continue;
    auto cid_it = line_to_cid_.find(kv.first);
    if (cid_it == line_to_cid_.end()) {
      spdlog::warn("Missing cid for symbolic branch {}", kv.first);
      return nullopt;
    }
    auto c_it = conds_.find(cid_it->second);
    if (c_it == conds_.end()) {
      spdlog::warn("Missing cond entry for cid {} (branch {})", cid_it->second,
                   kv.first);
      return nullopt;
    }
    parser_->add_constraints_as_task(c_it->second.label, kv.second ? 1 : 0, task_id); //TODO: we do not use kv.second. kv.second means if we want to take it or not on the ast side. However, here we need the direction on the llvm ir side.
    symbolic_meta.emplace(kv.first, c_it->second);
    spdlog::debug("Added symbolic constraint for branch {} -> {}", kv.first,
                  kv.second);
  }

  std::vector<uint8_t> buf = input_bytes;
  std::vector<uint64_t> solve_tasks;

  symsan::Z3ParserSolver::solution_t solutions;
  auto st = parser_->solve_task(task_id, cfg_.solve_timeout_ms, solutions);
  if (st != symsan::Z3ParserSolver::opt_sat &&
      st != symsan::Z3ParserSolver::nested_sat &&
      st != symsan::Z3ParserSolver::opt_sat_nested_timeout &&
      st != symsan::Z3ParserSolver::opt_sat_nested_unsat) {
    return nullopt;
  }
  for (auto const &sol : solutions) {
    if (sol.offset < buf.size()) {
      buf[sol.offset] = sol.val;
    }
  }

  std::string new_input = materialize_input(buf);
  if (new_input.empty())
    return nullopt;

  RunResult verify;
  if (!run_once(new_input, true, verify))
    return nullopt;

  // verify requested branch directions are satisfied
  for (auto const &kv : branch_trace) {
    auto it = verify.branches.find(kv.first);
    if (it == verify.branches.end() || !it->second.seen || it->second.last_result != kv.second) {
      spdlog::debug("Verification failed for branch {} (expected {} but {}{})",
                    kv.first, kv.second,
                    (it == verify.branches.end() || !it->second.seen)
                        ? "branch missing"
                        : "observed = ",
                    (it == verify.branches.end() || !it->second.seen)
                        ? ""
                        : (it->second.last_result ? "true" : "false"));
      return nullopt;
    }
  }

  return verify;
}

Status RLDriver::HandleTrace(const rl::TraceRequest &req,
                             rl::TraceResponse *resp) {
  std::lock_guard<std::mutex> lock(mu_);

  spdlog::info("Trace request line_to_reach={} branches={}", req.line_to_reach(),
               req.branch_traces_size());

  // Reset state for each new trace request to ensure fresh symbolic execution
  input_queue_.clear();
  input_queue_.push_back(cfg_.input_path);
  branches_.clear();
  conds_.clear();
  line_to_cid_.clear();
  branch_inputs_.clear();
  last_input_path_.clear();

  std::unordered_map<uint64_t, bool> branch_trace;
  for (auto const &kv : req.branch_traces()) {
    auto lt = line_table_.find(kv.first);
    if (lt != line_table_.end() && lt->second.is_if) {
      branch_trace[kv.first] = kv.second;
    }
  }

  RunResult last_run;
  bool covered = explore_until_covered(branch_trace, last_run);

  std::unordered_map<uint64_t, bool> sym_map;
  std::unordered_map<uint64_t, bool> non_sym_map;
  bool reached = false;
  bool sat = true;
  bool timeout = false;

  if (!covered) {
    spdlog::warn("Unable to reach some if branches.");
    timeout = true;
    sat = false;
  } else {
    if (!branch_trace.empty()) {
      std::vector<uint8_t> buf;
      std::ifstream file(cfg_.input_path, std::ios::binary | std::ios::ate);
      auto size = file.tellg();
      file.seekg(0, std::ios::beg);
      std::vector<uint8_t> buffer(static_cast<size_t>(size));
      file.read((char*)(buffer.data()), size);
      if(auto trace_run = solve_for_trace(branch_trace, buffer)){
        last_run = *trace_run;
      }else{
        sat = false;
      }
    }

    // rebuild maps after solving/verification run
    for (auto const &kv : branch_trace) {
      if (!build_branch_maps(kv.first, sym_map, non_sym_map, last_run.branches)) {
        spdlog::debug("Branch {} missing from final run when building response",
                      kv.first);
      }
    }

    if (last_run.line_hits.count(req.line_to_reach()))
      reached = true;
  }

  resp->set_reached(reached);
  if (timeout) {
    resp->set_timeout(true);
  } else {
    resp->set_sat(sat);
  }

  auto *sym_field = resp->mutable_symbolic_branch_trace();
  for (auto const &kv : sym_map) {
    (*sym_field)[kv.first] = kv.second;
  }
  auto *non_sym_field = resp->mutable_non_symbolic_branch_trace();
  for (auto const &kv : non_sym_map) {
    (*non_sym_field)[kv.first] = kv.second;
  }

  return Status::OK;
}

Status RLDriver::HandleReadLines(rl::ReadLinesResponse *resp) {
  std::lock_guard<std::mutex> lock(mu_);
  auto *lines = resp->mutable_lines();
  for (auto const &kv : line_table_) {
    rl::LineInfo info;
    info.set_file(kv.second.file);
    info.set_line(kv.second.line);
    info.set_is_if(kv.second.is_if);
    (*lines)[kv.first] = info;
  }
  return Status::OK;
}

class RLDriverService final : public rl::RLDriver::Service {
public:
  explicit RLDriverService(RLDriver &driver) : driver_(driver) {}

  Status Trace(ServerContext *, const rl::TraceRequest *req,
               rl::TraceResponse *resp) override {
    return driver_.HandleTrace(*req, resp);
  }

  Status ReadLines(ServerContext *, const rl::ReadLinesRequest *,
                   rl::ReadLinesResponse *resp) override {
    return driver_.HandleReadLines(resp);
  }

private:
  RLDriver &driver_;
};

void RLDriver::Serve() {
  if (!init()) {
    return;
  }

  if (!cfg_.workdir.empty() && cfg_.workdir != ".") {
    if (chdir(cfg_.workdir.c_str()) != 0) {
      fprintf(stderr, "Failed to chdir to %s: %s\n",
              cfg_.workdir.c_str(), strerror(errno));
    }
  }
  load_linecov();

  RLDriverService service(*this);
  ServerBuilder builder;
  builder.AddListeningPort(cfg_.listen_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  if (!server) {
    fprintf(stderr, "Failed to start gRPC server on %s\n", cfg_.listen_addr.c_str());
    return;
  }
  fprintf(stderr, "RL driver listening on %s\n", cfg_.listen_addr.c_str());
  server->Wait();
}

} // namespace

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s --config <file> [--listen <addr>]\n", prog);
  fprintf(stderr, "  --config <file>   Configuration file path\n");
  fprintf(stderr, "  --listen <addr>   Listen address (default: 0.0.0.0:50051)\n");
}

static bool parse_config_file(const std::string &path, Config &cfg) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;
    std::string key = line.substr(0, pos);
    std::string val = line.substr(pos + 1);
    if (key == "target")
      cfg.target = val;
    else if (key == "input")
      cfg.input_path = val;
    else if (key == "output_dir")
      cfg.output_dir = val;
    else if (key == "workdir")
      cfg.workdir = val;
    else if (key == "listen_addr")
      cfg.listen_addr = val;
    else if (key == "use_stdin")
      cfg.use_stdin = (val == "1" || val == "true");
    else if (key == "debug")
      cfg.debug = (val == "1" || val == "true");
    else if (key == "bounds_check")
      cfg.bounds_check = (val == "1" || val == "true");
    else if (key == "solve_ub")
      cfg.solve_ub = (val == "1" || val == "true");
    else if (key == "run_timeout_ms")
      cfg.run_timeout_ms = static_cast<unsigned>(std::stoul(val));
    else if (key == "solve_timeout_ms")
      cfg.solve_timeout_ms = static_cast<unsigned>(std::stoul(val));
    else if (key == "max_runs")
      cfg.max_runs = static_cast<unsigned>(std::stoul(val));
    else if (key == "args") {
      std::stringstream ss(val);
      std::string a;
      while (ss >> a)
        cfg.args.push_back(a);
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  std::string config_path;
  std::string listen_addr;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
      listen_addr = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  if (config_path.empty()) {
    fprintf(stderr, "Error: --config is required\n");
    usage(argv[0]);
    return 1;
  }

  Config cfg;
  if (!parse_config_file(config_path, cfg)) {
    fprintf(stderr, "Failed to read config %s\n", config_path.c_str());
    return 1;
  }

  // Command line overrides config file
  if (!listen_addr.empty()) {
    cfg.listen_addr = listen_addr;
  }

  spdlog::cfg::load_env_levels();
  RLDriver driver(cfg);
  driver.Serve();
  return 0;
}
