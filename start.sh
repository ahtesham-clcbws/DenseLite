#!/bin/bash

# start.sh - Single entry point for DenseLite
# Handles compilation, pre-flight checks, and log management.

LOG_FILE="denselite.log"
MAX_LOG_LINES=5000

echo "======================================" >> "$LOG_FILE"
echo "=== DenseLite Startup: $(date) ===" | tee -a "$LOG_FILE"
echo "======================================" >> "$LOG_FILE"

# 1. Check .env configuration
if [ ! -f ".env" ]; then
    echo "[!] .env file not found. Copying from env.example..." | tee -a "$LOG_FILE"
    cp env.example .env
fi

# 2. Check Models Directory
if [ ! -d "models" ] || [ -z "$(ls -A models 2>/dev/null)" ]; then
    echo "[-] WARNING: 'models' directory is missing or empty. Local fallback inference will fail." | tee -a "$LOG_FILE"
    echo "    Please download required GGUF files to the models/ directory." | tee -a "$LOG_FILE"
else
    echo "[+] Models directory detected." | tee -a "$LOG_FILE"
fi

# 3. Check if able to run properly (Compile if needed)
if [ ! -f "build/DenseLite" ]; then
    echo "[!] Compiled binary not found in build/DenseLite. Starting build process..." | tee -a "$LOG_FILE"
    mkdir -p build && cd build
    echo "--> Running CMake..." | tee -a "../$LOG_FILE"
    cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. >> "../$LOG_FILE" 2>&1
    
    echo "--> Compiling with make..." | tee -a "../$LOG_FILE"
    make -j$(nproc) DenseLite >> "../$LOG_FILE" 2>&1
    cd ..
    
    if [ ! -f "build/DenseLite" ]; then
        echo "[!] Compilation failed. Please check $LOG_FILE for details." | tee -a "$LOG_FILE"
        exit 1
    fi
    echo "[+] Build successful." | tee -a "$LOG_FILE"
fi

# 4. Limit log size (keep last N lines to prevent indefinite growth)
if [ -f "$LOG_FILE" ]; then
    tail -n $MAX_LOG_LINES "$LOG_FILE" > "${LOG_FILE}.tmp"
    mv "${LOG_FILE}.tmp" "$LOG_FILE"
fi

# 5. Run the engine and pipe output to the log
echo "[+] Starting DenseLite engine..." | tee -a "$LOG_FILE"
echo "    API will be available at http://localhost:9501" | tee -a "$LOG_FILE"
echo "--------------------------------------" >> "$LOG_FILE"

# Execute the binary, piping stderr and stdout to tee for dual-output (console + limited log file)
cd build && ./DenseLite 2>&1 | tee -a "../$LOG_FILE"
