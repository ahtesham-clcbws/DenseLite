#!/bin/bash
gdb -batch -ex "run" -ex "bt" ./build/DenseLite > gdb_output.log 2>&1 &
GDB_PID=$!
sleep 5
echo "Sending curl request..."
curl -s http://localhost:9501/health
curl -s -N http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "denselite",
    "messages": [
      {"role": "user", "content": "Hello"}
    ],
    "stream": false
  }'
wait $GDB_PID
cat gdb_output.log
