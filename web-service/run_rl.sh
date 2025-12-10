#!/bin/bash
# 启动 RL Driver Web Service
# 先启动后端 gRPC 服务，再启动 FastAPI Web 服务

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 启动后端
echo "Starting RL backends..."
bash "${SCRIPT_DIR}/start_backends.sh" start

# 等待后端启动
sleep 2

# 检查后端状态
bash "${SCRIPT_DIR}/start_backends.sh" status

# 启动 FastAPI
echo ""
echo "Starting FastAPI web service..."
cd "$SCRIPT_DIR"

# 使用 uvicorn 启动
if command -v uvicorn &> /dev/null; then
    uvicorn app_rl:app --host 0.0.0.0 --port 8000 --reload
else
    python -m uvicorn app_rl:app --host 0.0.0.0 --port 8000 --reload
fi
