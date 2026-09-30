#!/bin/bash
# e2e_stress.sh: End-to-End stress test for DenseLite context and tool loops

set -e

API_URL="http://localhost:9501/v1/chat/completions"

echo "=== DENSELITE E2E STRESS TEST ==="
echo "Testing Search Fallback & Semantic Extraction..."
curl -s -N --max-time 120 $API_URL \
  -H "Content-Type: application/json" \
  -d '{
    "model": "denselite",
    "messages": [
      {"role": "user", "content": "I want you to remember this rule: always use the repository pattern."}
    ],
    "stream": false
  }' > /dev/null

echo "✅ Semantic Extraction Request Sent"

echo "Testing Tool Loop..."
curl -s -N --max-time 120 $API_URL \
  -H "Content-Type: application/json" \
  -d '{
    "model": "denselite",
    "messages": [
      {"role": "user", "content": "Search the local codebase for MemoryConsolidator."}
    ],
    "stream": false
  }' > /dev/null
echo "✅ Tool Loop Request Sent"

echo "Testing Context Overflow Mechanics..."
# Generate a very large string to simulate a massive context
LARGE_STRING=$(python3 -c "print('The quick brown fox jumps over the lazy dog. ' * 5000)")
PAYLOAD=$(python3 -c "import json; print(json.dumps({'model': 'denselite', 'messages': [{'role': 'user', 'content': f'Summarize this text: $LARGE_STRING'}], 'stream': False}))")

curl -s -X POST --max-time 120 $API_URL \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD" > /dev/null

echo "✅ Context Overflow Request Sent"

echo "Stress test suite execution complete."
