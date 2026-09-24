# DenseLite

**DenseLite** is a hyper-optimized, C++ based multi-model orchestration gateway designed to dynamically load, route, and execute large language models, vector embeddings, speech recognition, and image generation locally. It acts as an incredibly fast, edge-optimized "local brain".

## Features
- **Dynamic Hot-Loading**: Models are swapped in and out of RAM on-demand to protect hardware limits.
- **Architecture Agnostic GGUF Parsing**: Capable of dynamically detecting and parsing `.context_length`, `.block_count`, and other metadata for *any* standard GGUF architecture out-of-the-box (Qwen, Llama, Nomic, Mistral, etc.).
- **Multi-Modal Support**: Integrated pipelines for text (Qwen / SmolLM), speech-to-text (Whisper), image generation (Stable Diffusion 1.5), and vector embeddings (Nomic).
- **Semantic Intent Routing**: Employs a specialized "Needle" router to intelligently route requests to the most appropriate loaded model.
- **Hardware Enforced**: Built-in strict hardware limit enforcement to prevent out-of-memory errors on local/edge hardware.
- **No Path Hardcoding**: Auto-resolves execution environments dynamically.

## Quick Start

### 1. Setup Environment
Clone the repository and copy the example environment file:
```bash
git clone https://github.com/ahtesham-clcbws/DenseLite.git
cd DenseLite
cp .env.local .env
```

### 2. Download Models
All required models should be placed inside a `models/` directory at the root of the project.
You can find the direct HuggingFace download links for each supported model inside the `.env.local` file. Simply download the `.gguf`, `.safetensors`, and `.bin` files and place them according to the `MODEL_*_FILE` paths defined in the environment.

### 3. Build from Source
DenseLite uses CMake and heavily depends on AVX2/AVX512 optimizations for its internal `Zvec` engine.

```bash
mkdir build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
make -j4 DenseLite
```

### 4. Run the Gateway
Execute the compiled binary from the `build` directory:
```bash
./DenseLite
```
The API server will spin up on `http://localhost:9501`.

## Extending DenseLite
DenseLite is built to be easily extended! The `server.cpp` initialization dynamically loads models based on your `.env` configuration. You can easily plug new models in, or extend the `DenseLiteEngine` class to handle new routing logic for specialized use cases.

## License
MIT License. **Anyone can use this in commercial or personal projects.** 
*Condition:* My name (Ahtesham) and this GitHub repository link must be mentioned/attributed in your project or codebase. See `LICENSE` for details.
