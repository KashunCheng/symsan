#!/bin/bash
# 启动三个 RL gRPC 后端服务，分别监听不同端口
# control_temp: 50051
# dummy: 50052
# complex: 50053

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
RLDRIVER_BIN="${REPO_ROOT}/build/bin/RLDriver"
EXAMPLE_RL_DIR="${REPO_ROOT}/example_rl"

# 检查 RLDriver 是否存在
if [ ! -f "$RLDRIVER_BIN" ]; then
    echo "Error: RLDriver not found at $RLDRIVER_BIN"
    echo "Please run ./build.sh first"
    exit 1
fi

# 定义后端配置
declare -A BACKENDS=(
    ["control_temp"]="50051"
    ["dummy"]="50052"
    ["complex"]="50053"
)

# PID 文件目录
PID_DIR="${SCRIPT_DIR}/.pids"
mkdir -p "$PID_DIR"

# 日志目录
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "$LOG_DIR"

start_backend() {
    local name=$1
    local port=$2
    local workdir="${EXAMPLE_RL_DIR}/${name}"
    local conf_file="${workdir}/rl.conf"
    local pid_file="${PID_DIR}/${name}.pid"
    local log_file="${LOG_DIR}/${name}.log"
    
    # 检查目录是否存在
    if [ ! -d "$workdir" ]; then
        echo "Error: Directory $workdir not found"
        return 1
    fi
    
    # 检查配置文件
    if [ ! -f "$conf_file" ]; then
        echo "Error: Config file $conf_file not found"
        return 1
    fi
    
    # 检查是否已经在运行
    if [ -f "$pid_file" ]; then
        local old_pid=$(cat "$pid_file")
        if kill -0 "$old_pid" 2>/dev/null; then
            echo "[$name] Already running (PID: $old_pid)"
            return 0
        else
            rm -f "$pid_file"
        fi
    fi
    
    echo "[$name] Starting on port $port..."
    
    # 启动后端
    cd "$workdir"
    
    # 先构建（如果需要）
    if [ -f "build.sh" ]; then
        bash build.sh > "${log_file}.build" 2>&1
    fi
    
    # 启动 RLDriver
    SPDLOG_LEVEL=info "$RLDRIVER_BIN" --config "$conf_file" --listen "127.0.0.1:${port}" > "$log_file" 2>&1 &
    local pid=$!
    echo $pid > "$pid_file"
    
    # 等待一下检查是否启动成功
    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        echo "[$name] Started successfully (PID: $pid, Port: $port)"
    else
        echo "[$name] Failed to start. Check $log_file for details"
        rm -f "$pid_file"
        return 1
    fi
}

stop_backend() {
    local name=$1
    local pid_file="${PID_DIR}/${name}.pid"
    
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            echo "[$name] Stopping (PID: $pid)..."
            kill "$pid"
            sleep 1
            if kill -0 "$pid" 2>/dev/null; then
                echo "[$name] Force killing..."
                kill -9 "$pid"
            fi
        fi
        rm -f "$pid_file"
        echo "[$name] Stopped"
    else
        echo "[$name] Not running"
    fi
}

status_backend() {
    local name=$1
    local port=$2
    local pid_file="${PID_DIR}/${name}.pid"
    
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            echo "[$name] Running (PID: $pid, Port: $port)"
        else
            echo "[$name] Dead (stale PID file)"
            rm -f "$pid_file"
        fi
    else
        echo "[$name] Not running"
    fi
}

case "${1:-start}" in
    start)
        echo "Starting all RL backends..."
        for name in "${!BACKENDS[@]}"; do
            start_backend "$name" "${BACKENDS[$name]}"
        done
        echo "All backends started."
        ;;
    stop)
        echo "Stopping all RL backends..."
        for name in "${!BACKENDS[@]}"; do
            stop_backend "$name"
        done
        echo "All backends stopped."
        ;;
    restart)
        $0 stop
        sleep 2
        $0 start
        ;;
    status)
        echo "Backend status:"
        for name in "${!BACKENDS[@]}"; do
            status_backend "$name" "${BACKENDS[$name]}"
        done
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
