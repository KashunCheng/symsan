#!/usr/bin/env python3
"""
自动：
1. 调用 ReadLines 一次，拿到所有行信息
2. 从中挑出指定源码行号对应的 line_id / is_if
3. 选出 is_if == False 的那个作为 line_to_reach
4. 其它和它一起都作为 branch traces
5. 调用 Trace 并打印和 rl_client.py 类似的 JSON 输出
"""
import os
from typing import List, Dict, Tuple

import grpc  # 已在 rl_client.py 里用到，这里也直接用
import rl_client  # 就是你贴的那个文件，文件名为 rl_client.py


# 你关心的源码行号（就是之前你手动执行 --line 50 / 54 / 61 / 72）
TARGET_LINES: List[int] = [50, 54, 61, 72]

# gRPC endpoint
ENDPOINT = "127.0.0.1:50051"


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
    nodes: List[Tuple[int, object]]
) -> Tuple[int, List[Tuple[int, bool]]]:
    """
    根据 nodes 构造：
      - line_to_reach: is_if == False 的那个 line_id
      - branches: [(line_id, bool), ...]  包含 true 和 false 全部
    """
    branches: List[Tuple[int, bool]] = []
    line_to_reach: int | None = None

    for lid, info in nodes:
        is_if = bool(info.is_if)
        branches.append((lid, is_if))
        if not is_if:
            if line_to_reach is None:
                line_to_reach = lid
            else:
                # 如果真的遇到多个 is_if == False，可以按需改成报错或保留第一个
                print(
                    f"[WARN] 已有 line_to_reach={line_to_reach}，"
                    f"又遇到 is_if==False 的 {lid}，先保留第一个"
                )

    if line_to_reach is None:
        raise RuntimeError("没有任何 is_if == False 的节点，无法设置 line_to_reach")

    return line_to_reach, branches


def run_trace(stub, rl_pb2, line_to_reach: int, branches: List[Tuple[int, bool]], id_to_line: Dict[int, int]):
    """真正调用 Trace，并以和 rl_client.py 相同格式打印 JSON。"""
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

    # 把 line_id 翻译回行号再打印一次
    def translate_trace(trace: dict) -> dict:
        return {f"line_{id_to_line.get(int(k), k)}": v for k, v in trace.items()}

    out_with_lines = {
        "reached": resp.reached,
        "status": status,
        "symbolic_branch_trace": translate_trace(resp.symbolic_branch_trace),
        "non_symbolic_branch_trace": translate_trace(resp.non_symbolic_branch_trace),
    }

    print("\n[INFO] 翻译后的输出（line_id -> 行号）:")
    print(json.dumps(out_with_lines, indent=2, ensure_ascii=False))


def main():
    stub, rl_pb2 = make_stub(ENDPOINT)

    print(f"[INFO] target source lines = {TARGET_LINES}")
    nodes = collect_nodes_for_lines(stub, rl_pb2, TARGET_LINES)

    # 构建 line_id -> 源码行号 的映射
    id_to_line: Dict[int, int] = {lid: info.line for lid, info in nodes}

    for lid, info in nodes:
        print(
            f"[NODE] line_id={lid}, file={info.file}, line={info.line}, is_if={info.is_if}"
        )

    line_to_reach, branches = build_trace_args(nodes)

    print(f"[INFO] line_to_reach = {line_to_reach}")
    print(f"[INFO] branches = {branches}")

    run_trace(stub, rl_pb2, line_to_reach, branches, id_to_line)


if __name__ == "__main__":
    os.system("cd complex && bash ./build.sh && cd .. && cd dummy &&  bash ./build.sh && cd ..")
    main()
