#!/bin/bash
# memory_integrity.sh: Validate INSERT mechanics over multiple turns

set -e

API_URL="http://localhost:9501/v1/chat/completions"

echo "=== DENSELITE MEMORY INTEGRITY TEST ==="
echo "Simulating a multi-turn session to ensure UPSERT replaces turns cleanly..."

# Turn 1
curl -s -N --max-time 120 $API_URL \
  -H "Content-Type: application/json" \
  -d '{
    "model": "denselite",
    "messages": [
      {"role": "user", "content": "Hello, my name is Alice."}
    ],
    "stream": false
  }' > /dev/null
echo "✅ Turn 1 Sent"

# Turn 2
curl -s -N --max-time 120 $API_URL \
  -H "Content-Type: application/json" \
  -d '{
    "model": "denselite",
    "messages": [
      {"role": "user", "content": "Hello, my name is Alice."},
      {"role": "assistant", "content": "Hello Alice! How can I help you today?"},
      {"role": "user", "content": "Can you summarize our conversation?"}
    ],
    "stream": false
  }' > /dev/null
echo "✅ Turn 2 Sent"

echo "Memory integrity test complete. Check DB for valid turn_count without duplicates."
