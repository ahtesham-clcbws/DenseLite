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

# 2. Check and Auto-Download Models
mkdir -p models/needle3
mkdir -p models/whisper
mkdir -p models/sd15

# Function to download model if missing
download_if_missing() {
    local file=$1
    local url=$2
    if [ ! -f "$file" ]; then
        echo "[-] Model missing: $file" | tee -a "$LOG_FILE"
        echo "    Downloading directly from HuggingFace..." | tee -a "$LOG_FILE"
        wget -q --show-progress "$url" -O "$file"
    fi
}

echo "[+] Verifying core and requested models from .env..." | tee -a "$LOG_FILE"

# Dynamically parse .env and download missing models
grep "^MODEL_.*_FILE=" .env | while read -r line; do
    var_name=$(echo "$line" | cut -d'=' -f1)
    file_path=$(echo "$line" | cut -d'=' -f2 | tr -d '"')
    
    # Needle3 is bundled in git, skip downloading it
    if [[ "$file_path" == *"needle3"* ]]; then
        continue
    fi
    
    # Find matching URL variable
    url_var_name="${var_name/_FILE/_URL}"
    url=$(grep "^${url_var_name}=" .env | cut -d'=' -f2 | tr -d '"')
    
    if [ -n "$url" ]; then
        download_if_missing "models/$file_path" "$url"
    fi
done


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
