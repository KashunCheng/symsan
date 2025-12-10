"""
RL Driver 包装器
负责调用 gRPC 后端服务进行分支路径追踪并返回 reward
"""
import sys
import json
import logging
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple, Optional, Any

# 添加 example_rl 目录到 Python 路径
REPO_ROOT = Path(__file__).resolve().parents[1]
EXAMPLE_RL_DIR = REPO_ROOT / "example_rl"
sys.path.insert(0, str(EXAMPLE_RL_DIR))

# 现在可以导入 auto_trace 模块
import grpc

# Basic logger
_logger = logging.getLogger("rl_driver")
if not _logger.handlers:
    handler = logging.StreamHandler()
    formatter = logging.Formatter("[%(asctime)s] %(levelname)s %(message)s")
    handler.setFormatter(formatter)
    _logger.addHandler(handler)
_logger.setLevel(logging.INFO)

# 程序到端口的映射
PROGRAM_ENDPOINTS = {
    "control_temp": "127.0.0.1:50051",
    "dummy": "127.0.0.1:50052",
    "complex": "127.0.0.1:50053",
}


def update_status(result_dir: str, status: str, error: Optional[str] = None, result: Optional[Dict] = None):
    """更新任务状态文件"""
    status_path = Path(result_dir) / "status.json"
    
    # 读取现有状态
    if status_path.exists():
        with open(status_path, "r") as f:
            status_data = json.load(f)
    else:
        status_data = {}
    
    # 更新状态
    status_data["status"] = status
    status_data["updated_at"] = datetime.now().isoformat()
    
    if error:
        status_data["error"] = error
    
    if result:
        status_data["result"] = result
    
    # 如果状态是运行中，设置开始时间
    if status == "running" and "started_at" not in status_data:
        status_data["started_at"] = datetime.now().isoformat()
    
    # 如果是完成或失败状态，设置完成时间
    if status in ["completed", "failed"]:
        status_data["completed_at"] = datetime.now().isoformat()
    
    # 写入状态文件
    with open(status_path, "w") as f:
        json.dump(status_data, f, indent=2)


def make_stub(endpoint: str):
    """创建 gRPC stub"""
    # 导入 rl_client 模块
    import rl_client
    out_dir = rl_client.compile_proto()
    rl_pb2, rl_pb2_grpc = rl_client.load_stubs(out_dir)
    channel = grpc.insecure_channel(endpoint)
    stub = rl_pb2_grpc.RLDriverStub(channel)
    return stub, rl_pb2, channel


def get_all_lines(stub, rl_pb2) -> Dict[int, Dict[str, Any]]:
    """获取所有行信息"""
    req = rl_pb2.ReadLinesRequest()
    resp = stub.ReadLines(req)
    return {
        lid: {
            "file": info.file,
            "line": info.line,
            "is_if": info.is_if
        }
        for lid, info in resp.lines.items()
    }


def call_trace(
    stub,
    rl_pb2,
    line_to_reach: int,
    branches: Dict[int, bool],
    all_lines: Dict[int, Dict[str, Any]]
) -> Tuple[float, Dict[str, Any]]:
    """
    调用 Trace RPC 并计算 reward
    
    Args:
        stub: gRPC stub
        rl_pb2: protobuf 模块
        line_to_reach: 目标行的 line_id
        branches: 分支方向，格式为 {line_id: direction(True/False)}
        all_lines: 所有行信息
    
    Returns:
        (reward, details) - reward 分数和详细信息
    """
    # 导入 reward 计算相关常量和函数
    from auto_trace import (
        calculate_reward,
        BRANCH_TRACE_TYPE,
        REWARD_BRANCH_MATCH,
        REWARD_BRANCH_MISMATCH,
        REWARD_REACHED,
        REWARD_IS_IF_TRUE,
        REWARD_IS_IF_FALSE,
    )
    
    req = rl_pb2.TraceRequest()
    req.line_to_reach = line_to_reach
    for lid, val in branches.items():
        req.branch_traces[lid] = val
    
    resp = stub.Trace(req)
    
    # 构建映射
    id_to_line = {lid: info["line"] for lid, info in all_lines.items()}
    id_to_is_if = {lid: info["is_if"] for lid, info in all_lines.items()}
    
    # 计算 reward
    reward, reward_details = calculate_reward(
        input_branches=branches,
        symbolic_branch_trace=dict(resp.symbolic_branch_trace),
        non_symbolic_branch_trace=dict(resp.non_symbolic_branch_trace),
        reached=resp.reached,
        id_to_line=id_to_line,
        id_to_is_if=id_to_is_if
    )
    
    # 确定状态
    if resp.HasField("timeout"):
        status = "timeout"
    else:
        status = "sat" if resp.sat else "unsat"
    
    # 翻译 trace
    def translate_trace(trace: dict) -> dict:
        result = {}
        for k, v in trace.items():
            line_key = id_to_line.get(int(k), k)
            readable_val = BRANCH_TRACE_TYPE.get(v, v) if isinstance(v, int) else v
            result[line_key] = readable_val
        return result
    
    details = {
        "reached": resp.reached,
        "status": status,
        "reward": reward,
        "reward_details": reward_details,
        "symbolic_branch_trace": translate_trace(resp.symbolic_branch_trace),
        "non_symbolic_branch_trace": translate_trace(resp.non_symbolic_branch_trace),
    }
    
    return reward, details


def run_rl_task(
    task_id: str,
    program: str,
    line_to_reach: int,
    branches: Dict[int, bool],
    result_dir: str,
):
    """
    执行 RL 任务
    
    Args:
        task_id: 任务 ID
        program: 程序名称 (control_temp, dummy, complex)
        line_to_reach: 目标行号（源码行号）
        branches: 分支方向配置，格式为 {源码行号: 方向(True/False)}
        result_dir: 结果目录
    """
    try:
        # 更新状态为运行中
        update_status(result_dir, "running")
        _logger.info("[%s] RL task starting (program=%s, line_to_reach=%d, branches=%s)",
                     task_id, program, line_to_reach, branches)
        
        # 获取端点
        endpoint = PROGRAM_ENDPOINTS.get(program)
        if not endpoint:
            raise ValueError(f"Unknown program: {program}. Must be one of {list(PROGRAM_ENDPOINTS.keys())}")
        
        # 连接 gRPC 后端
        _logger.info("[%s] Connecting to %s", task_id, endpoint)
        stub, rl_pb2, channel = make_stub(endpoint)
        
        # 获取所有行信息
        all_lines = get_all_lines(stub, rl_pb2)
        _logger.info("[%s] Got %d lines from backend", task_id, len(all_lines))
        
        # 构建 源码行号 -> line_id 的映射
        line_to_id = {info["line"]: lid for lid, info in all_lines.items()}
        
        # 转换源码行号到 line_id
        line_to_reach_id = line_to_id.get(line_to_reach)
        if line_to_reach_id is None:
            raise ValueError(f"Line {line_to_reach} not found in program {program}")
        
        branches_by_id = {}
        for src_line, direction in branches.items():
            lid = line_to_id.get(src_line)
            if lid is None:
                _logger.warning("[%s] Branch line %d not found, skipping", task_id, src_line)
                continue
            branches_by_id[lid] = direction
        
        # 调用 Trace
        reward, details = call_trace(stub, rl_pb2, line_to_reach_id, branches_by_id, all_lines)
        
        # 关闭 channel
        channel.close()
        
        # 保存结果
        result_data = {
            "task_id": task_id,
            "program": program,
            "line_to_reach": line_to_reach,
            "branches": {str(k): v for k, v in branches.items()},
            "reward": reward,
            "details": details,
            "timestamp": datetime.now().isoformat(),
        }
        
        rewards_path = Path(result_dir) / "rewards.json"
        with open(rewards_path, "w") as f:
            json.dump(result_data, f, indent=2, ensure_ascii=False)
        
        # 更新状态为完成
        update_status(result_dir, "completed", result=result_data)
        _logger.info("[%s] RL task completed. Reward: %f", task_id, reward)
        
    except grpc.RpcError as e:
        error_msg = f"gRPC error: {e.code()}: {e.details()}"
        update_status(result_dir, "failed", error=error_msg)
        _logger.error("[%s] RL task failed: %s", task_id, error_msg)
        
    except Exception as e:
        import traceback
        error_msg = f"{type(e).__name__}: {str(e)}\n{traceback.format_exc()}"
        update_status(result_dir, "failed", error=error_msg)
        _logger.exception("[%s] RL task failed", task_id)
