#!/usr/bin/env python3
"""
Simple gRPC client for the RL driver.

Uses grpcio/grpcio-tools to generate Python stubs from driver/rl/rl.proto
at runtime into a temporary directory, then issues either:
  - read-lines: fetch the line table
  - trace: send branch predictions and target line

Example:
  .venv/bin/python example_rl/rl_client.py --endpoint localhost:50051 read-lines
  .venv/bin/python example_rl/rl_client.py --endpoint localhost:50051 trace \\
      --line-to-reach 123456 --branch 1111:true --branch 2222:false
"""

import argparse
import json
import sys
import tempfile
from pathlib import Path

import grpc
from grpc_tools import protoc


def compile_proto() -> Path:
  """Compile rl.proto to a temp directory and return the directory."""
  repo_root = Path(__file__).resolve().parents[1]
  proto_path = repo_root / "driver" / "rl" / "rl.proto"
  out_dir = Path(tempfile.mkdtemp(prefix="rl_proto_"))
  args = [
      "protoc",
      f"-I{proto_path.parent}",
      f"--python_out={out_dir}",
      f"--grpc_python_out={out_dir}",
      str(proto_path),
  ]
  if protoc.main(args) != 0:
    raise RuntimeError("Failed to generate protobuf stubs")
  sys.path.insert(0, str(out_dir))
  return out_dir


def load_stubs(out_dir: Path):
  """Import generated modules after compilation."""
  import importlib

  rl_pb2 = importlib.import_module("rl_pb2")
  rl_pb2_grpc = importlib.import_module("rl_pb2_grpc")
  return rl_pb2, rl_pb2_grpc


def parse_branch(kv: str):
  """Parse CLI --branch line_id:bool."""
  if ":" not in kv:
    raise argparse.ArgumentTypeError("branch must be line_id:true|false")
  line, val = kv.split(":", 1)
  try:
    line_id = int(line, 0)
  except ValueError as e:
    raise argparse.ArgumentTypeError(str(e))
  v_lower = val.lower()
  if v_lower in ("1", "true", "t", "yes"):
    direction = True
  elif v_lower in ("0", "false", "f", "no"):
    direction = False
  else:
    raise argparse.ArgumentTypeError("branch value must be true/false")
  return line_id, direction


def cmd_read_lines(stub, rl_pb2, args):
  line = args.line
  resp = stub.ReadLines(rl_pb2.ReadLinesRequest())
  print(json.dumps({lid: {"file": info.file, "line": info.line, "is_if": info.is_if}
                    for lid, info in resp.lines.items() if line is None or line == info.line},
                   indent=2))


def cmd_trace(stub, rl_pb2, args):
  req = rl_pb2.TraceRequest()
  req.line_to_reach = args.line_to_reach
  for lid, val in args.branch:
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
  print(json.dumps(out, indent=2))


def main():
  parser = argparse.ArgumentParser(description="RL driver gRPC client")
  parser.add_argument("--endpoint", default="localhost:50051",
                      help="gRPC server endpoint (host:port)")
  sub = parser.add_subparsers(dest="cmd", required=True)

  sub_read = sub.add_parser("read-lines", help="Fetch line table")
  sub_read.add_argument("--line", type=int, required=False,
                        help="Only show entries with this line number")
  sub_read.set_defaults(cmd_func=lambda stub, pb, a: cmd_read_lines(stub, pb))

  sub_trace = sub.add_parser("trace", help="Send trace request")
  sub_trace.add_argument("--line-to-reach", type=int, required=True,
                         help="Target line_id to reach")
  sub_trace.add_argument("--branch", type=parse_branch, action="append",
                         default=[], help="Branch prediction line_id:true|false")
  sub_trace.set_defaults(cmd_func=lambda stub, pb, a: cmd_trace(stub, pb, a))

  args = parser.parse_args()

  out_dir = compile_proto()
  rl_pb2, rl_pb2_grpc = load_stubs(out_dir)

  with grpc.insecure_channel(args.endpoint) as channel:
    stub = rl_pb2_grpc.RLDriverStub(channel)
    if args.cmd == "read-lines":
      cmd_read_lines(stub, rl_pb2, args)
    elif args.cmd == "trace":
      cmd_trace(stub, rl_pb2, args)
    else:
      parser.error("unknown command")


if __name__ == "__main__":
  main()
