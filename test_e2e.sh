#!/bin/bash
# End-to-end test for DenseLite server
# Simulates exactly what Zed Editor sends

set -e

echo "=== BUILDING SERVER ==="
cd /mnt/apollo/Apollo4/DenseLite
mkdir -p build && cd build && cmake .. && make -j4 DenseLite
cd ..
echo "Build successful."

echo ""
echo "=== STARTING SERVER ==="
./build/DenseLite &
SERVER_PID=$!
sleep 3

# Cleanup on exit
trap "kill $SERVER_PID 2>/dev/null; exit" EXIT

echo ""
echo "=== TEST 1: Health Check ==="
HEALTH=$(curl -s http://localhost:9501/health)
echo "Response: $HEALTH"
if echo "$HEALTH" | grep -q '"status":"ok"'; then
    echo "✅ PASS: Health check"
else
    echo "❌ FAIL: Health check"
    exit 1
fi

echo ""
echo "=== TEST 2: Simple string content (basic OpenAI format) ==="
RESPONSE=$(curl -s -N --max-time 120 http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"denselite","messages":[{"role":"user","content":"What is 2+2?"}],"stream":true,"max_completion_tokens":50}')
echo "Raw SSE response:"
echo "$RESPONSE"
echo ""

# Check that response contains valid SSE data lines
if echo "$RESPONSE" | grep -q 'data:.*"content"'; then
    echo "✅ PASS: Simple content - got SSE data chunks"
else
    echo "❌ FAIL: Simple content - no SSE data chunks"
fi

# Check that response does NOT contain <|im_end|>
if echo "$RESPONSE" | grep -q 'im_end'; then
    echo "❌ FAIL: EOS token leaked into output"
else
    echo "✅ PASS: EOS tokens properly filtered"
fi

# Check [DONE] marker present
if echo "$RESPONSE" | grep -q '\[DONE\]'; then
    echo "✅ PASS: [DONE] marker present"
else
    echo "❌ FAIL: [DONE] marker missing"
fi

# Verify each data line is valid JSON
echo ""
echo "Verifying JSON validity of each SSE chunk..."
INVALID_JSON=0
while IFS= read -r line; do
    if [[ "$line" == data:\ \{* ]]; then
        json_part="${line#data: }"
        if ! echo "$json_part" | python3 -c "import sys,json; json.load(sys.stdin)" 2>/dev/null; then
            echo "❌ Invalid JSON: $json_part"
            INVALID_JSON=1
        fi
    fi
done <<< "$RESPONSE"
if [ "$INVALID_JSON" -eq 0 ]; then
    echo "✅ PASS: All SSE chunks are valid JSON"
else
    echo "❌ FAIL: Some SSE chunks had invalid JSON"
fi

echo ""
echo "=== TEST 3: Zed-style array content format ==="
RESPONSE2=$(curl -s -N --max-time 120 http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model":"denselite",
    "messages":[{
        "role":"user",
        "content":[
            {"type":"text","text":"[@readme.md (1:28)](file:///mnt/BWS/task_manager_app/readme.md#L1:28)"},
            {"type":"text","text":"<context>\nSome context here\n\n"},
            {"type":"text","text":"Hello, can you help me write a Python function?"},
            {"type":"text","text":"</context>"}
        ]
    }],
    "stream":true,
    "max_completion_tokens":50
  }')
echo "Raw SSE response:"
echo "$RESPONSE2"
echo ""

if echo "$RESPONSE2" | grep -q 'data:.*"content"'; then
    echo "✅ PASS: Array content - got SSE data chunks"
else
    echo "❌ FAIL: Array content - no SSE data chunks"
fi

# Verify JSON validity
INVALID_JSON2=0
while IFS= read -r line; do
    if [[ "$line" == data:\ \{* ]]; then
        json_part="${line#data: }"
        if ! echo "$json_part" | python3 -c "import sys,json; json.load(sys.stdin)" 2>/dev/null; then
            echo "❌ Invalid JSON: $json_part"
            INVALID_JSON2=1
        fi
    fi
done <<< "$RESPONSE2"
if [ "$INVALID_JSON2" -eq 0 ]; then
    echo "✅ PASS: All SSE chunks are valid JSON"
else
    echo "❌ FAIL: Some SSE chunks had invalid JSON"
fi

echo ""
echo "=== TEST 4: Response with newlines (JSON escape test) ==="
RESPONSE3=$(curl -s -N --max-time 120 http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"denselite","messages":[{"role":"user","content":"Write hello world in python"}],"stream":true,"max_completion_tokens":80}')
echo "Raw SSE response:"
echo "$RESPONSE3"
echo ""

# Verify every JSON chunk
INVALID_JSON3=0
while IFS= read -r line; do
    if [[ "$line" == data:\ \{* ]]; then
        json_part="${line#data: }"
        if ! echo "$json_part" | python3 -c "import sys,json; json.load(sys.stdin)" 2>/dev/null; then
            echo "❌ Invalid JSON in code response: $json_part"
            INVALID_JSON3=1
        fi
    fi
done <<< "$RESPONSE3"
if [ "$INVALID_JSON3" -eq 0 ]; then
    echo "✅ PASS: Code response - all JSON valid (newlines properly escaped)"
else
    echo "❌ FAIL: Code response - JSON broken (newline escaping failed)"
fi

echo ""
echo "========================================="
echo "  ALL TESTS COMPLETE"
echo "========================================="
