#!/usr/bin/env python3
"""
自动：
1. 调用 ReadLines 一次，拿到所有行信息
2. 从中挑出指定源码行号对应的 line_id / is_if
3. 选出 is_if == False 的那个作为 line_to_reach
4. 其它和它一起都作为 branch traces
5. 调用 Trace 并打印和 rl_client.py 类似的 JSON 输出
"""
import argparse
import os
from typing import List, Dict, Tuple

import grpc  # 已在 rl_client.py 里用到，这里也直接用
import rl_client  # 就是你贴的那个文件，文件名为 rl_client.py


# ==================== Reward 常量配置 ====================
# 分支方向匹配奖励
REWARD_BRANCH_MATCH = 1.0
# 分支方向不匹配惩罚
REWARD_BRANCH_MISMATCH = -1.0
# 到达目标行奖励
REWARD_REACHED = 10.0
# 分支是有效 if 语句奖励
REWARD_IS_IF_TRUE = 0.5
# 分支不是有效 if 语句惩罚
REWARD_IS_IF_FALSE = -1.0 #弄大点防止卡bug
# =========================================================


# enum BranchTraceType {
#   BRANCH_TRACE_NOT_REACHED = 0;
#   BRANCH_TRACE_TAKEN = 1;
#   BRANCH_TRACE_NOT_TAKEN = 2;
#   BRANCH_TRACE_TAKEN_AND_NOT_TAKEN = 3;
# }

# BranchTraceType 枚举值到可读描述的映射
BRANCH_TRACE_TYPE = {
    0: "NOT_REACHED",
    1: "TAKEN",
    2: "NOT_TAKEN",
    3: "TAKEN_AND_NOT_TAKEN",
}


def calculate_reward(
    input_branches: Dict[int, bool],
    symbolic_branch_trace: Dict[int, int],
    non_symbolic_branch_trace: Dict[int, int],
    reached: bool,
    id_to_line: Dict[int, int],
    id_to_is_if: Dict[int, bool] = None
) -> Tuple[float, Dict[str, any]]:
    """
    计算 reward 分数
    
    规则：
    - 如果输入的分支在 non_symbolic_branch_trace 里：不加分也不减分（+0）
    - 如果输入的分支在 symbolic_branch_trace 里：
      - 方向匹配（如输入 63:true，返回 63:TAKEN）：+REWARD_BRANCH_MATCH
      - 方向不匹配：+REWARD_BRANCH_MISMATCH
    - reached == True：+REWARD_REACHED
    - 分支 is_if == True：+REWARD_IS_IF_TRUE
    - 分支 is_if == False：+REWARD_IS_IF_FALSE
    
    Args:
        input_branches: 输入的分支方向，格式为 {line_id: direction(True/False)}
        symbolic_branch_trace: 后端返回的符号分支追踪结果，格式为 {line_id: BranchTraceType}
        non_symbolic_branch_trace: 后端返回的非符号分支追踪结果
        reached: 是否到达目标行
        id_to_line: line_id 到源码行号的映射
        id_to_is_if: line_id 到 is_if 的映射（可选）
    
    Returns:
        (total_score, details) - 总分数和详细计算过程
    """
    if id_to_is_if is None:
        id_to_is_if = {}
    
    total_score = 0.0
    details = {
        "branch_scores": [],
        "is_if_scores": [],
        "reached_bonus": 0.0,
        "total": 0.0
    }
    
    for line_id, input_direction in input_branches.items():
        src_line = id_to_line.get(line_id, line_id)
        
        # 检查 is_if 属性并计分
        if line_id in id_to_is_if:
            is_if = id_to_is_if[line_id]
            if is_if:
                is_if_score = REWARD_IS_IF_TRUE
                is_if_reason = "is_if=True"
            else:
                is_if_score = REWARD_IS_IF_FALSE
                is_if_reason = "is_if=False"
            total_score += is_if_score
            details["is_if_scores"].append({
                "line": src_line,
                "is_if": is_if,
                "score": is_if_score,
                "reason": is_if_reason
            })
        
        # 检查是否在 non_symbolic_branch_trace 中
        if line_id in non_symbolic_branch_trace:
            details["branch_scores"].append({
                "line": src_line,
                "type": "non_symbolic",
                "score": 0,
                "reason": "在 non_symbolic 中，不计分"
            })
            continue
        
        # 检查是否在 symbolic_branch_trace 中
        if line_id in symbolic_branch_trace:
            trace_type = symbolic_branch_trace[line_id]
            # TAKEN = 1, NOT_TAKEN = 2
            # input_direction: True 对应 TAKEN(1), False 对应 NOT_TAKEN(2)
            expected_type = 1 if input_direction else 2  # TAKEN=1, NOT_TAKEN=2
            
            # 判断方向是否匹配
            if trace_type == expected_type:
                score = REWARD_BRANCH_MATCH
                reason = f"匹配: 输入={input_direction}, 返回={BRANCH_TRACE_TYPE.get(trace_type, trace_type)}"
            elif trace_type == 3:  # TAKEN_AND_NOT_TAKEN - 两个方向都走过
                score = REWARD_BRANCH_MATCH
                reason = f"匹配(双向): 输入={input_direction}, 返回=TAKEN_AND_NOT_TAKEN"
            else:
                score = REWARD_BRANCH_MISMATCH
                reason = f"不匹配: 输入={input_direction}, 返回={BRANCH_TRACE_TYPE.get(trace_type, trace_type)}"
            
            total_score += score
            details["branch_scores"].append({
                "line": src_line,
                "type": "symbolic",
                "score": score,
                "reason": reason
            })
        else:
            # 不在任何追踪结果中
            details["branch_scores"].append({
                "line": src_line,
                "type": "not_found",
                "score": 0,
                "reason": "未在返回结果中找到"
            })
    
    # reached 奖励
    if reached:
        total_score += REWARD_REACHED
        details["reached_bonus"] = REWARD_REACHED
    
    details["total"] = total_score
    
    return total_score, details


def make_stub(endpoint: str):
    """复用 rl_client.py 里的编译 proto + 加载 stub 逻辑，创建 gRPC stub。"""
    out_dir = rl_client.compile_proto()
    rl_pb2, rl_pb2_grpc = rl_client.load_stubs(out_dir)
    channel = grpc.insecure_channel(endpoint)
    stub = rl_pb2_grpc.RLDriverStub(channel)
    return stub, rl_pb2


def collect_nodes_for_lines(
    stub, rl_pb2, target_lines: List[int]
) -> List[Tuple[int, object]]:
    """
    从 ReadLines 里一次性拿到所有行，然后按源码行号过滤出我们要的条目。

    返回：[(line_id:int, info), ...]
    """
    req = rl_pb2.ReadLinesRequest()
    resp = stub.ReadLines(req)

    want = set(target_lines)
    found: List[Tuple[int, object]] = []

    # resp.lines 是一个 map<int64, LineInfo>
    for lid, info in resp.lines.items():
        if info.line in want:
            found.append((lid, info))

    # 简单 sanity check
    lines_covered = {info.line for _, info in found}
    missing = want - lines_covered
    if missing:
        raise RuntimeError(f"这些源码行没在 ReadLines 结果中找到: {sorted(missing)}")

    return found


def build_trace_args(
    nodes: List[Tuple[int, object]],
    line_to_reach_src: int,
    branch_directions: Dict[int, bool] = None
) -> Tuple[int, List[Tuple[int, bool]]]:
    """
    根据 nodes 构造：
      - line_to_reach: 明确指定的源码行号对应的 line_id
      - branches: [(line_id, direction), ...]  其他节点作为分支
      
    line_to_reach_src: 目标行的源码行号
    branch_directions: 可选的字典，指定每个 line_id 的分支方向（true/false）
                      如果未提供，默认使用 True
    """
    branches: List[Tuple[int, bool]] = []
    line_to_reach: int | None = None

    if branch_directions is None:
        branch_directions = {}

    for lid, info in nodes:
        if info.line == line_to_reach_src:
            # 这是明确指定的目标行
            line_to_reach = lid
        else:
            # 其他节点作为分支
            direction = branch_directions.get(lid, True)
            branches.append((lid, direction))

    if line_to_reach is None:
        raise RuntimeError(f"指定的 line_to_reach 源码行 {line_to_reach_src} 没有在 nodes 中找到")

    return line_to_reach, branches


def run_trace(stub, rl_pb2, line_to_reach: int, branches: List[Tuple[int, bool]], id_to_line: Dict[int, int], id_to_is_if: Dict[int, bool] = None):
    """真正调用 Trace，并以和 rl_client.py 相同格式打印 JSON。"""
    if id_to_is_if is None:
        id_to_is_if = {}
    
    req = rl_pb2.TraceRequest()
    req.line_to_reach = line_to_reach
    for lid, val in branches:
        req.branch_traces[lid] = val

    resp = stub.Trace(req)

    if resp.HasField("timeout"):
        status = "timeout"
    else:
        status = "sat" if resp.sat else "unsat"

    out = {
        "reached": resp.reached,
        "status": status,
        "symbolic_branch_trace": dict(resp.symbolic_branch_trace),
        "non_symbolic_branch_trace": dict(resp.non_symbolic_branch_trace),
    }

    import json

    print(json.dumps(out, indent=2))

    # 把 line_id 翻译回行号，并将枚举值转换为可读描述
    def translate_trace(trace: dict) -> dict:
        result = {}
        for k, v in trace.items():
            line_key = f"{id_to_line.get(int(k), k)}"
            readable_val = BRANCH_TRACE_TYPE.get(v, v) if isinstance(v, int) else v
            result[line_key] = readable_val
        return result

    out_with_lines = {
        "reached": resp.reached,
        "status": status,
        "symbolic_branch_trace": translate_trace(resp.symbolic_branch_trace),
        "non_symbolic_branch_trace": translate_trace(resp.non_symbolic_branch_trace),
    }

    print("\n[INFO] 翻译后的输出（line_id -> 行号）:")
    print(json.dumps(out_with_lines, indent=2, ensure_ascii=False))

    # 计算 reward 分数
    input_branches = {lid: val for lid, val in branches}
    reward, reward_details = calculate_reward(
        input_branches=input_branches,
        symbolic_branch_trace=dict(resp.symbolic_branch_trace),
        non_symbolic_branch_trace=dict(resp.non_symbolic_branch_trace),
        reached=resp.reached,
        id_to_line=id_to_line,
        id_to_is_if=id_to_is_if
    )
    
    print(f"\n[REWARD] 总分: {reward}")
    print("[REWARD] 详细计算:")
    for is_if_score in reward_details.get("is_if_scores", []):
        print(f"  - 行 {is_if_score['line']}: {is_if_score['score']:+.1f} ({is_if_score['reason']})")
    for branch_score in reward_details["branch_scores"]:
        print(f"  - 行 {branch_score['line']}: {branch_score['score']:+.1f} ({branch_score['reason']})")
    if reward_details["reached_bonus"] > 0:
        print(f"  - reached 奖励: +{reward_details['reached_bonus']}")
    
    return reward, reward_details


def parse_branch_arg(branch_str: str) -> Tuple[int, bool]:
    """
    解析分支参数，格式为 "line:direction"
    例如: "32:true" -> (32, True), "38:false" -> (38, False)
    """
    parts = branch_str.split(":")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"分支格式错误: {branch_str}，应为 'line:true' 或 'line:false'")
    
    try:
        line = int(parts[0])
    except ValueError:
        raise argparse.ArgumentTypeError(f"行号必须是整数: {parts[0]}")
    
    direction_str = parts[1].lower()
    if direction_str in ("true", "1", "t", "taken"):
        direction = True
    elif direction_str in ("false", "0", "f", "not_taken"):
        direction = False
    else:
        raise argparse.ArgumentTypeError(f"分支方向必须是 true/false: {parts[1]}")
    
    return line, direction


def trace_branch_path(
    line_to_reach: int,
    branch_directions_by_line: Dict[int, bool],
    endpoint: str = "127.0.0.1:50051"
):
    """
    执行分支路径追踪
    
    通过 gRPC 调用 RL 驱动服务，尝试找到一条能够到达指定目标行的执行路径，
    同时满足给定的分支方向约束。
    
    Args:
        line_to_reach: 目标行的源码行号
        branch_directions_by_line: 分支方向配置，格式为 {源码行号: 方向(True/False)}
        endpoint: gRPC 服务端点
    
    Returns:
        None (结果会打印到标准输出)
    
    
# 调用示例
from auto_trace import trace_branch_path
trace_branch_path(
    line_to_reach=85,
    branch_directions_by_line={
        32: False,
        38: False,
        63: True,
        65: False
    },
    endpoint="127.0.0.1:50051"
)
    """
    stub, rl_pb2 = make_stub(endpoint)

    # 所有相关的行号：line_to_reach + 所有分支行
    target_lines = [line_to_reach] + list(branch_directions_by_line.keys())
    print(f"[INFO] line_to_reach (source line) = {line_to_reach}")
    print(f"[INFO] branch lines = {list(branch_directions_by_line.keys())}")
    print(f"[INFO] all target source lines = {target_lines}")
    
    nodes = collect_nodes_for_lines(stub, rl_pb2, target_lines)

    # 构建 line_id -> 源码行号 的映射
    id_to_line: Dict[int, int] = {lid: info.line for lid, info in nodes}
    # 构建 源码行号 -> line_id 的映射
    line_to_id: Dict[int, int] = {info.line: lid for lid, info in nodes}
    # 构建 line_id -> is_if 的映射
    id_to_is_if: Dict[int, bool] = {lid: info.is_if for lid, info in nodes}

    for lid, info in nodes:
        print(
            f"[NODE] line_id={lid}, file={info.file}, line={info.line}, is_if={info.is_if}"
        )

    # 将源码行号的分支方向配置转换为 line_id 的分支方向
    branch_directions: Dict[int, bool] = {}
    for src_line, direction in branch_directions_by_line.items():
        if src_line in line_to_id:
            branch_directions[line_to_id[src_line]] = direction
            print(f"[BRANCH] line {src_line} (id={line_to_id[src_line]}) -> {direction}")

    line_to_reach_id, branches = build_trace_args(nodes, line_to_reach, branch_directions)

    print(f"[INFO] line_to_reach = {line_to_reach_id}")
    print(f"[INFO] branches = {branches}")
    
    # 打印可以直接执行的命令
    branch_args = " ".join([f"--branch {lid}:{str(val).lower()}" for lid, val in branches])
    print(f"\n[CMD] python rl_client.py --endpoint {endpoint} trace --line-to-reach {line_to_reach_id} {branch_args}")

    run_trace(stub, rl_pb2, line_to_reach_id, branches, id_to_line, id_to_is_if)


def cli():
    """命令行入口"""
    parser = argparse.ArgumentParser(
        description="自动调用 RL gRPC 服务进行分支追踪",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 指定目标行和分支方向
  python auto_trace.py --line-to-reach 85 --branch 32:false --branch 38:false --branch 63:true --branch 65:false
  
  # 指定自定义端点
  python auto_trace.py --endpoint 127.0.0.1:50052 --line-to-reach 85 --branch 32:false
        """
    )
    
    parser.add_argument(
        "--endpoint", "-e",
        default="127.0.0.1:50051",
        help="gRPC 服务端点 (默认: 127.0.0.1:50051)"
    )
    
    parser.add_argument(
        "--line-to-reach", "-l",
        type=int,
        required=True,
        help="目标行的源码行号"
    )
    
    parser.add_argument(
        "--branch", "-b",
        action="append",
        dest="branches",
        default=[],
        metavar="LINE:DIRECTION",
        help="分支方向配置，格式为 'line:true' 或 'line:false'，可多次指定"
    )
    
    args = parser.parse_args()
    
    # 解析分支配置
    branch_directions: Dict[int, bool] = {}
    for branch_str in args.branches:
        line, direction = parse_branch_arg(branch_str)
        branch_directions[line] = direction
    
    trace_branch_path(
        line_to_reach=args.line_to_reach,
        branch_directions_by_line=branch_directions,
        endpoint=args.endpoint
    )


if __name__ == "__main__":
    cli()
    # # 基本用法
    # python auto_trace.py --line-to-reach 85 --branch 32:false --branch 38:false --branch 63:true --branch 65:false

    # # 使用简写
    # python auto_trace.py -l 85 -b 32:false -b 38:false -b 63:true -b 65:false

    # # 指定自定义端点
    # python auto_trace.py --endpoint 127.0.0.1:50052 -l 85 -b 32:false

    # # 查看帮助
    # python auto_trace.py --help
