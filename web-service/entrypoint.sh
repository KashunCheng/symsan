#!/bin/bash
set -e

# 如果提供了 CLOUDFLARE_TUNNEL_TOKEN，启动 cloudflared tunnel
if [ -n "$CLOUDFLARE_TUNNEL_TOKEN" ]; then
    echo "Starting cloudflared tunnel..."
    cloudflared tunnel --no-autoupdate run --token "$CLOUDFLARE_TUNNEL_TOKEN" &
    sleep 2
    echo "Cloudflared tunnel started"
fi

echo "Starting RL Driver Web Service..."

# 启动 RL 后端服务
echo "Starting RL backends..."
bash /app/start_backends.sh start
sleep 2

# 检查后端状态
bash /app/start_backends.sh status

# 启动 uvicorn
echo "Starting uvicorn..."
exec uvicorn app_rl:app --host 0.0.0.0 --port 8000
