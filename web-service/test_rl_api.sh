#!/bin/bash
# 测试 RL Driver Web Service API

BASE_URL="${1:-http://localhost:8000}"

echo "Testing RL Driver Web Service at $BASE_URL"
echo "=============================================="

# 1. 健康检查
echo ""
echo "1. Health check..."
curl -s "$BASE_URL/health" | python -m json.tool

# 2. 列出程序
echo ""
echo "2. List programs..."
curl -s "$BASE_URL/api/programs" | python -m json.tool

# 3. 获取 control_temp 的行信息
echo ""
echo "3. Get lines for control_temp..."
curl -s "$BASE_URL/api/lines/control_temp" | python -m json.tool

# 4. 同步 trace 请求
echo ""
echo "4. Sync trace request..."
curl -s -X POST "$BASE_URL/api/trace/sync" \
    -H "Content-Type: application/json" \
    -d '{
        "program": "control_temp",
        "line_to_reach": 85,
        "branches": {
            "32": false,
            "38": false,
            "63": true,
            "65": false
        }
    }' | python -m json.tool

# 5. 异步 trace 请求
echo ""
echo "5. Async trace request..."
RESPONSE=$(curl -s -X POST "$BASE_URL/api/trace" \
    -H "Content-Type: application/json" \
    -d '{
        "program": "control_temp",
        "line_to_reach": 85,
        "branches": {
            "32": false,
            "38": false,
            "63": true,
            "65": false
        }
    }')
echo "$RESPONSE" | python -m json.tool

# 获取 task_id
TASK_ID=$(echo "$RESPONSE" | python -c "import sys, json; print(json.load(sys.stdin)['task_id'])" 2>/dev/null)
if [ -n "$TASK_ID" ]; then
    echo ""
    echo "6. Check task status (task_id=$TASK_ID)..."
    sleep 2
    curl -s "$BASE_URL/api/status/$TASK_ID" | python -m json.tool
fi

echo ""
echo "Done!"
