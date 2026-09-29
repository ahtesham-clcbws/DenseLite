#!/bin/bash
# DenseLite RSS & Process Memory Breakdown Profiler
# Analyzes physical RSS, PSS, Private, and Shared mappings from /proc/$PID/smaps_rollup

PID=$(pgrep -f "DenseLite" | head -n 1)
if [ -z "$PID" ]; then
    echo "[-] Error: DenseLite daemon is not currently running."
    exit 1
fi

echo "======================================================="
echo " DenseLite Process Memory Breakdown (PID: $PID)"
echo "======================================================="

if [ -f "/proc/$PID/smaps_rollup" ]; then
    awk '
    /Rss:/ {rss=$2}
    /Pss:/ {pss=$2}
    /Shared_Clean:/ {sc=$2}
    /Shared_Dirty:/ {sd=$2}
    /Private_Clean:/ {pc=$2}
    /Private_Dirty:/ {pd=$2}
    /Referenced:/ {ref=$2}
    /Anonymous:/ {anon=$2}
    END {
        printf "Total RSS:         %8.2f MB\n", rss / 1024
        printf "Proportional (PSS):%8.2f MB\n", pss / 1024
        printf "Private Dirty:     %8.2f MB  (Active heap, KV cache, buffers)\n", pd / 1024
        printf "Private Clean:     %8.2f MB\n", pc / 1024
        printf "Shared Memory:     %8.2f MB  (mmap shared / GGUF weights)\n", (sc + sd) / 1024
        printf "Anonymous Pages:   %8.2f MB\n", anon / 1024
    }' /proc/$PID/smaps_rollup
else
    # Fallback to ps if smaps_rollup unavailable
    ps -p "$PID" -o pid,vsz,rss,comm
fi
echo "======================================================="
