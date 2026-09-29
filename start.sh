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
    
    # 1. Select Main Qwen Model
    echo ""
    echo "Select Main Qwen Model (General & Reasoning):"
    select QWEN_MAIN in "Qwen2.5-1.5B-Abliterated (Recommended for >= 8GB RAM)" "Qwen2.5-0.5B-Abliterated (For low-spec systems)"; do
        case $QWEN_MAIN in
            "Qwen2.5-1.5B-Abliterated (Recommended for >= 8GB RAM)" ) QWEN_MAIN_FILE="Qwen2.5-1.5B-Instruct-abliterated.Q8_0.gguf"; QWEN_MAIN_URL="https://huggingface.co/hf-audio/qwen2.5-1.5b-instruct-abliterated-gguf/resolve/main/qwen2.5-1.5b-instruct-abliterated.q8_0.gguf"; break;;
            "Qwen2.5-0.5B-Abliterated (For low-spec systems)" ) QWEN_MAIN_FILE="Qwen2.5-0.5B-Instruct-abliterated.Q8_0.gguf"; QWEN_MAIN_URL="https://huggingface.co/hf-audio/qwen2.5-0.5b-instruct-abliterated-gguf/resolve/main/qwen2.5-0.5b-instruct-abliterated.q8_0.gguf"; break;;
        esac
    done

    # 2. Select Coder Qwen Model
    echo ""
    echo "Select Qwen Coder Model:"
    select QWEN_CODER in "Qwen2.5-Coder-1.5B-Abliterated (Recommended)" "Qwen2.5-Coder-0.5B-Abliterated (Faster, less accurate)"; do
        case $QWEN_CODER in
            "Qwen2.5-Coder-1.5B-Abliterated (Recommended)" ) QWEN_CODER_FILE="Qwen2.5-Coder-1.5B-Instruct-abliterated-Q8_0.gguf"; QWEN_CODER_URL="https://huggingface.co/hf-audio/qwen2.5-coder-1.5b-instruct-abliterated-gguf/resolve/main/qwen2.5-coder-1.5b-instruct-abliterated-q8_0.gguf"; break;;
            "Qwen2.5-Coder-0.5B-Abliterated (Faster, less accurate)" ) QWEN_CODER_FILE="Qwen2.5-Coder-0.5B-Instruct-abliterated-Q8_0.gguf"; QWEN_CODER_URL="https://huggingface.co/hf-audio/qwen2.5-coder-0.5b-instruct-abliterated-gguf/resolve/main/qwen2.5-coder-0.5b-instruct-abliterated-q8_0.gguf"; break;;
        esac
    done

    # 3. Select SmolLM2 Model
    echo ""
    echo "Select SmolLM2 Model (Context Compression):"
    select SMOLLM in "SmolLM2-360M (Standard)" "SmolLM2-135M (Minimal)"; do
        case $SMOLLM in
            "SmolLM2-360M (Standard)" ) SMOLLM_FILE="SmolLM2-360M-Instruct-Q8_0.gguf"; SMOLLM_URL="https://huggingface.co/HuggingFaceTB/SmolLM2-360M-Instruct-GGUF/resolve/main/smollm2-360m-instruct-q8_0.gguf"; break;;
            "SmolLM2-135M (Minimal)" ) SMOLLM_FILE="SmolLM2-135M-Instruct-Q8_0.gguf"; SMOLLM_URL="https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct-GGUF/resolve/main/smollm2-135m-instruct-q8_0.gguf"; break;;
        esac
    done

    # 4. Select Nomic Embed Model
    echo ""
    echo "Select Nomic Embed Model:"
    select NOMIC in "nomic-embed-text-v1.5 (Modern)" "nomic-embed-text-v1.0 (Legacy)"; do
        case $NOMIC in
            "nomic-embed-text-v1.5 (Modern)" ) NOMIC_FILE="nomic-embed-text-v1.5.Q8_0.gguf"; NOMIC_URL="https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q8_0.gguf"; break;;
            "nomic-embed-text-v1.0 (Legacy)" ) NOMIC_FILE="nomic-embed-text-v1.0.Q8_0.gguf"; NOMIC_URL="https://huggingface.co/nomic-ai/nomic-embed-text-v1.0-GGUF/resolve/main/nomic-embed-text-v1.0.Q8_0.gguf"; break;;
        esac
    done

    echo "[+] Generating .env file with your selections..."
    cp env.example .env
    
    # Inject selections into .env using sed
    sed -i "s|^MODEL_QWEN_MAIN_FILE=.*|MODEL_QWEN_MAIN_FILE=\"$QWEN_MAIN_FILE\"|" .env
    sed -i "s|^MODEL_QWEN_MAIN_URL=.*|MODEL_QWEN_MAIN_URL=\"$QWEN_MAIN_URL\"|" .env
    
    sed -i "s|^MODEL_QWEN_CODER_FILE=.*|MODEL_QWEN_CODER_FILE=\"$QWEN_CODER_FILE\"|" .env
    sed -i "s|^MODEL_QWEN_CODER_URL=.*|MODEL_QWEN_CODER_URL=\"$QWEN_CODER_URL\"|" .env
    
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

if [ ! -f "models/modernbert/model.onnx" ]; then
    echo "[+] Downloading internal ModernBERT Zero-Shot Intent Router..." | tee -a "$LOG_FILE"
    mkdir -p models/modernbert
    curl -L -s -o models/modernbert/config.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/config.json
    curl -L -s -o models/modernbert/tokenizer.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/tokenizer.json
    curl -L -s -o models/modernbert/tokenizer_config.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/tokenizer_config.json
    curl -L -s -o models/modernbert/special_tokens_map.json https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/special_tokens_map.json
    curl -L -s -o models/modernbert/model.onnx https://huggingface.co/MoritzLaurer/ModernBERT-large-zeroshot-v2.0/resolve/main/onnx/model_int8.onnx
fi

# Function to download model if missing
download_if_missing() {
    local file_name=$1
    local url=$2
    local user_model="$HOME/.denselite/models/$file_name"
    local local_model="models/$file_name"

    if [ -f "$user_model" ] || [ -f "$local_model" ]; then
        return 0
    fi

    mkdir -p "$(dirname "$user_model")"
    echo "[-] Model missing: $file_name" | tee -a "$LOG_FILE"
    echo "    Downloading to ~/.denselite/models/$file_name..." | tee -a "$LOG_FILE"
    wget -q --show-progress "$url" -O "$user_model"
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
        download_if_missing "$file_path" "$url"
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
