#!/bin/bash

# start.sh - Single entry point for DenseLite
# Handles compilation, pre-flight checks, and log management.

LOG_FILE="denselite.log"
MAX_LOG_LINES=5000

echo "======================================" >> "$LOG_FILE"
echo "=== DenseLite Startup: $(date) ===" | tee -a "$LOG_FILE"
echo "======================================" >> "$LOG_FILE"

# 1. Check .env configuration and Interactively Setup
if [ ! -f ".env" ]; then
    echo "======================================"
    echo "=== DenseLite First-Time Setup ==="
    echo "======================================"
    
    TOTAL_RAM=$(awk '/MemTotal/ {printf "%.0f", $2/1024/1024}' /proc/meminfo)
    echo "[i] System RAM Detected: ${TOTAL_RAM}GB"
    echo ""
    echo "Please select the models you want to use. All models are Q8_0 quantized."
    
    # 1. Select Main Reasoner Model (Llama 3.2 1B Instruct)
    echo ""
    echo "Select Main Reasoner Model (General Reasoning & Orchestration):"
    select MAIN_CHOICE in "Llama-3.2-1B-Instruct-Abliterated Q4_K_M (Recommended, ~800MB)" "Llama-3.2-1B-Instruct-Abliterated Q8_0 (High Precision, ~1.3GB)"; do
        case $MAIN_CHOICE in
            "Llama-3.2-1B-Instruct-Abliterated Q4_K_M (Recommended, ~800MB)" )
                MAIN_FILE="Llama-3.2-1B-Instruct-abliterated.i1-Q4_K_M.gguf"
                MAIN_URL="https://huggingface.co/mradermacher/Llama-3.2-1B-Instruct-abliterated-i1-GGUF/resolve/main/Llama-3.2-1B-Instruct-abliterated.i1-Q4_K_M.gguf"
                break;;
            "Llama-3.2-1B-Instruct-Abliterated Q8_0 (High Precision, ~1.3GB)" )
                MAIN_FILE="Llama-3.2-1B-Instruct-abliterated.Q8_0.gguf"
                MAIN_URL="https://huggingface.co/mradermacher/Llama-3.2-1B-Instruct-abliterated-GGUF/resolve/main/Llama-3.2-1B-Instruct-abliterated.Q8_0.gguf"
                break;;
        esac
    done

    # 2. Select Coder Model (DeepSeek-R1 Distill Qwen 1.5B)
    echo ""
    echo "Select Coding Model (Code Synthesis & AST Logic):"
    select CODER_CHOICE in "DeepSeek-R1-Distill-Qwen-1.5B Q4_K_M (Recommended, ~1.1GB)" "DeepSeek-R1-Distill-Qwen-1.5B Q8_0 (High Precision, ~1.8GB)"; do
        case $CODER_CHOICE in
            "DeepSeek-R1-Distill-Qwen-1.5B Q4_K_M (Recommended, ~1.1GB)" )
                CODER_FILE="DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
                CODER_URL="https://huggingface.co/unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
                break;;
            "DeepSeek-R1-Distill-Qwen-1.5B Q8_0 (High Precision, ~1.8GB)" )
                CODER_FILE="DeepSeek-R1-Distill-Qwen-1.5B-Q8_0.gguf"
                CODER_URL="https://huggingface.co/unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q8_0.gguf"
                break;;
        esac
    done

    # 3. Select SmolLM2 Model (Context Slicing / Formatter)
    echo ""
    echo "Select Formatter Model (Context Slicing & Injection):"
    select SMOLLM_CHOICE in "SmolLM2-360M Q4_K_M (Standard, ~250MB)" "SmolLM2-135M Q4_K_M (Ultra-light, ~100MB)"; do
        case $SMOLLM_CHOICE in
            "SmolLM2-360M Q4_K_M (Standard, ~250MB)" )
                SMOLLM_FILE="smollm2-360m-instruct-q4_k_m.gguf"
                SMOLLM_URL="https://huggingface.co/mfuntowicz/SmolLM2-360M-Instruct-Q4_K_M-GGUF/resolve/main/smollm2-360m-instruct-q4_k_m.gguf"
                break;;
            "SmolLM2-135M Q4_K_M (Ultra-light, ~100MB)" )
                SMOLLM_FILE="smollm2-135m-instruct-q4_k_m.gguf"
                SMOLLM_URL="https://huggingface.co/mfuntowicz/SmolLM2-135M-Instruct-Q4_K_M-GGUF/resolve/main/smollm2-135m-instruct-q4_k_m.gguf"
                break;;
        esac
    done

    # 4. Select Nomic Embed Model
    echo ""
    echo "Select Vector Memory Embedder:"
    select NOMIC_CHOICE in "nomic-embed-text-v2-moe Q4_K_M (Modern MoE, Recommended)" "nomic-embed-text-v1.5 Q8_0 (Legacy Dense)"; do
        case $NOMIC_CHOICE in
            "nomic-embed-text-v2-moe Q4_K_M (Modern MoE, Recommended)" )
                NOMIC_FILE="nomic-embed-text-v2-moe.Q4_K_M.gguf"
                NOMIC_URL="https://huggingface.co/nomic-ai/nomic-embed-text-v2-moe-GGUF/resolve/main/nomic-embed-text-v2-moe.Q4_K_M.gguf"
                break;;
            "nomic-embed-text-v1.5 Q8_0 (Legacy Dense)" )
                NOMIC_FILE="nomic-embed-text-v1.5.Q8_0.gguf"
                NOMIC_URL="https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q8_0.gguf"
                break;;
        esac
    done

    echo "[+] Generating .env file with your selections..."
    cp env.example .env
    
    # Inject selections into .env using sed
    sed -i "s|^MODEL_MAIN_FILE=.*|MODEL_MAIN_FILE=\"$MAIN_FILE\"|" .env
    sed -i "s|^MODEL_MAIN_URL=.*|MODEL_MAIN_URL=\"$MAIN_URL\"|" .env
    
    sed -i "s|^MODEL_CODER_FILE=.*|MODEL_CODER_FILE=\"$CODER_FILE\"|" .env
    sed -i "s|^MODEL_CODER_URL=.*|MODEL_CODER_URL=\"$CODER_URL\"|" .env
    
    sed -i "s|^MODEL_SMOLLM2_FILE=.*|MODEL_SMOLLM2_FILE=\"$SMOLLM_FILE\"|" .env
    sed -i "s|^MODEL_SMOLLM2_URL=.*|MODEL_SMOLLM2_URL=\"$SMOLLM_URL\"|" .env
    
    sed -i "s|^MODEL_NOMIC_FILE=.*|MODEL_NOMIC_FILE=\"$NOMIC_FILE\"|" .env
    sed -i "s|^MODEL_NOMIC_URL=.*|MODEL_NOMIC_URL=\"$NOMIC_URL\"|" .env
    
    echo "[+] Configuration saved to .env" | tee -a "$LOG_FILE"
fi

# 2. Check and Auto-Download Models & Runtime Engines
mkdir -p "$HOME/.denselite/models"
mkdir -p "$HOME/.denselite/models/vision"
mkdir -p "$HOME/.denselite/models/whisper"
mkdir -p models/modernbert

if [ ! -d "dependencies/onnxruntime" ] || [ ! -f "dependencies/onnxruntime/lib/libonnxruntime.so" ]; then
    echo "[+] Downloading prebuilt ONNX Runtime C++ release..." | tee -a "$LOG_FILE"
    mkdir -p dependencies/onnxruntime
    curl -L -s https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-1.20.1.tgz | tar -xz -C dependencies/onnxruntime --strip-components=1
fi

if [ ! -f "models/modernbert/model.onnx" ] || [ ! -f "models/modernbert/model.safetensors" ]; then
    echo "[+] Downloading internal ModernBERT Zero-Shot Intent Router (MoritzLaurer/ModernBERT-large-zeroshot-v2.0)..." | tee -a "$LOG_FILE"
    mkdir -p models/modernbert
    curl -L -s -o models/modernbert/config.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/config.json
    curl -L -s -o models/modernbert/tokenizer.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/tokenizer.json
    curl -L -s -o models/modernbert/tokenizer_config.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/tokenizer_config.json
    curl -L -s -o models/modernbert/special_tokens_map.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/special_tokens_map.json
    curl -L -s -o models/modernbert/model.onnx https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/onnx/model_int8.onnx
    curl -L -s -o models/modernbert/model.safetensors https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/model.safetensors
    curl -L -s -o models/modernbert/README.md https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/raw/main/README.md
fi

# Function to download model if missing
download_if_missing() {
    local file_name=$1
    local url=$2
    local expected_sha256=$3
    local user_model="$HOME/.denselite/models/$file_name"
    local local_model="models/$file_name"
    local target_file=""

    if [ -f "$user_model" ]; then
        target_file="$user_model"
    elif [ -f "$local_model" ]; then
        target_file="$local_model"
    fi

    if [ -n "$target_file" ]; then
        if [ -n "$expected_sha256" ]; then
            local actual_sha=$(sha256sum "$target_file" 2>/dev/null | awk '{print $1}')
            if [ "$actual_sha" = "$expected_sha256" ]; then
                return 0
            else
                echo "[!] Integrity checksum mismatch on existing $file_name, re-downloading..." | tee -a "$LOG_FILE"
                rm -f "$target_file"
            fi
        else
            return 0
        fi
    fi

    mkdir -p "$(dirname "$user_model")"
    echo "[-] Model missing: $file_name" | tee -a "$LOG_FILE"
    echo "    Downloading to ~/.denselite/models/$file_name..." | tee -a "$LOG_FILE"
    wget -q --show-progress "$url" -O "$user_model"

    if [ -n "$expected_sha256" ]; then
        local actual_sha=$(sha256sum "$user_model" 2>/dev/null | awk '{print $1}')
        if [ "$actual_sha" != "$expected_sha256" ]; then
            echo "[!] FATAL: Downloaded model checksum verification failed for $file_name!" | tee -a "$LOG_FILE"
            rm -f "$user_model"
            return 1
        fi
        echo "[+] Model checksum verified: $file_name (SHA-256 match)" | tee -a "$LOG_FILE"
    fi
}

echo "[+] Verifying core and requested models from .env..." | tee -a "$LOG_FILE"

# Dynamically parse .env and download missing models
grep "^MODEL_.*_FILE=" .env | while read -r line; do
    var_name=$(echo "$line" | cut -d'=' -f1)
    file_path=$(echo "$line" | cut -d'=' -f2 | tr -d '"')
    
    # ModernBERT ONNX router is bundled in models/modernbert
    if [[ "$file_path" == *"modernbert"* ]]; then
        continue
    fi
    
    # Find matching URL and SHA256 variables
    url_var_name="${var_name/_FILE/_URL}"
    url=$(grep "^${url_var_name}=" .env | cut -d'=' -f2 | tr -d '"')
    sha_var_name="${var_name/_FILE/_SHA256}"
    sha=$(grep "^${sha_var_name}=" .env | cut -d'=' -f2 | tr -d '"')
    
    if [ -n "$url" ]; then
        download_if_missing "$file_path" "$url" "$sha"
    fi
done


# 3. Check if able to run properly (Compile if needed)
if [ ! -f "build/DenseLite" ] || [ ! -f "build/DenseLiteTray" ]; then
    echo "[!] Compiled binaries not found. Starting build process..." | tee -a "$LOG_FILE"
    cmake -B build -S . >> "$LOG_FILE" 2>&1
    cmake --build build -j$(nproc) >> "$LOG_FILE" 2>&1
    
    if [ ! -f "build/DenseLite" ] || [ ! -f "build/DenseLiteTray" ]; then
        echo "[!] Compilation failed. Please check $LOG_FILE for details." | tee -a "$LOG_FILE"
        exit 1
    fi
    echo "[+] Build successful." | tee -a "$LOG_FILE"
fi

# Pre-flight Database Verification & Self-Healing Bootstrap
echo "[i] Verifying SQLite databases and WAL configuration..." | tee -a "$LOG_FILE"
./build/DenseLite --init-db >> "$LOG_FILE" 2>&1

# 4. Limit log size (keep last N lines to prevent indefinite growth)
if [ -f "$LOG_FILE" ]; then
    tail -n $MAX_LOG_LINES "$LOG_FILE" > "${LOG_FILE}.tmp"
    mv "${LOG_FILE}.tmp" "$LOG_FILE"
fi

# 5. Ensure system tray icons are registered in user theme
if [ ! -f "$HOME/.local/share/icons/hicolor/32x32/apps/denselite_idle.png" ]; then
    for s in 16 22 24 32 48 64 128; do
        mkdir -p "$HOME/.local/share/icons/hicolor/${s}x${s}/apps"
        cp "src/tray/icons/${s}x${s}/apps/"*.png "$HOME/.local/share/icons/hicolor/${s}x${s}/apps/" 2>/dev/null || true
    done
    mkdir -p "$HOME/.local/share/icons/hicolor/scalable/apps"
    cp "src/tray/icons/scalable/apps/"*.svg "$HOME/.local/share/icons/hicolor/scalable/apps/" 2>/dev/null || true
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
fi

# 6. Launch the System Tray Supervisor (Inference engine remains idle until commanded)
echo "[+] Starting DenseLite System Tray & Control Plane..." | tee -a "$LOG_FILE"
echo "    Control Plane: http://127.0.0.1:9500" | tee -a "$LOG_FILE"
echo "    (Inference engine remains IDLE until started from Settings Panel or Tray)" | tee -a "$LOG_FILE"
echo "--------------------------------------" >> "$LOG_FILE"

# Execute the native Tray supervisor in foreground
./build/DenseLiteTray
