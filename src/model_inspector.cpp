#include "model_inspector.hpp"
#include "model.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <map>

constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"

std::string ModelInspector::format_param_count(uint64_t count) {
    std::ostringstream ss;
    if (count >= 1000000000ULL) {
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(count) / 1000000000.0) << "B";
    } else if (count >= 1000000ULL) {
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(count) / 1000000.0) << "M";
    } else {
        ss << count;
    }
    return ss.str();
}

std::string ModelInspector::quant_type_to_string(uint32_t type) {
    return tensor_type_name(static_cast<TensorType>(type));
}

bool ModelInspector::is_role_compatible(const std::string& arch, const std::string& role) {
    if (role == "general" || role == "coder" || role == "compressor" || role == "router") {
        return (arch == "qwen2" || arch == "llama" || arch == "gemma" || arch == "phi" || arch == "mistral" || arch == "bert" || arch == "modernbert");
    }
    if (role == "embedding") return (arch == "nomic-bert" || arch == "nomic-bert-moe" || arch == "bert" || arch == "modernbert");
    if (role == "audio_stt") return (arch == "whisper");
    if (role == "image_gen") return (arch == "diffusion" || arch == "unet" || arch == "stable-diffusion" || arch == "sd1");
    return false;
}

ModelInspectionResult ModelInspector::inspect(const std::string& file_path) {
    ModelInspectionResult res;
    if (!std::filesystem::exists(file_path)) {
        res.error_message = "File does not exist: " + file_path;
        return res;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        res.error_message = "Failed to open file: " + file_path;
        return res;
    }

    uint32_t magic = 0, version = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != GGUF_MAGIC) {
        res.error_message = "Invalid format: File is not a valid GGUF model.";
        return res;
    }

    file.read(reinterpret_cast<char*>(&version), 4);
    uint64_t tensor_count = 0, kv_count = 0;
    file.read(reinterpret_cast<char*>(&tensor_count), 8);
    file.read(reinterpret_cast<char*>(&kv_count), 8);

    auto read_str = [&file]() -> std::string {
        uint64_t len = 0;
        file.read(reinterpret_cast<char*>(&len), 8);
        if (len > 4096) return "";
        std::string s(len, '\0');
        file.read(&s[0], len);
        return s;
    };

    auto skip_val = [&file, &read_str](auto& self, uint32_t t) -> void {
        switch (t) {
            case 0: case 1: case 7: file.seekg(1, std::ios::cur); break;
            case 2: case 3: file.seekg(2, std::ios::cur); break;
            case 4: case 5: case 6: file.seekg(4, std::ios::cur); break;
            case 10: case 11: case 12: file.seekg(8, std::ios::cur); break;
            case 8: read_str(); break;
            case 9: {
                uint32_t arr_type = 0; uint64_t arr_len = 0;
                file.read(reinterpret_cast<char*>(&arr_type), 4);
                file.read(reinterpret_cast<char*>(&arr_len), 8);
                for (uint64_t j = 0; j < arr_len && file.good(); ++j) self(self, arr_type);
                break;
            }
            default: file.seekg(4, std::ios::cur); break;
        }
    };

    for (uint64_t i = 0; i < kv_count && file.good(); ++i) {
        std::string key = read_str();
        uint32_t type = 0;
        file.read(reinterpret_cast<char*>(&type), 4);
        if (key == "general.architecture" && type == 8) {
            res.architecture = read_str();
        } else if (key.find(".context_length") != std::string::npos && type == 4) {
            file.read(reinterpret_cast<char*>(&res.context_length), 4);
        } else {
            skip_val(skip_val, type);
        }
    }

    std::map<uint32_t, uint64_t> quant_counts;
    uint64_t total_params = 0;

    for (uint64_t i = 0; i < tensor_count && file.good(); ++i) {
        std::string tensor_name = read_str();
        uint32_t n_dims = 0;
        file.read(reinterpret_cast<char*>(&n_dims), 4);
        uint64_t elements = 1;
        for (uint32_t d = 0; d < n_dims; ++d) {
            uint64_t dim = 0;
            file.read(reinterpret_cast<char*>(&dim), 8);
            elements *= dim;
        }
        total_params += elements;
        uint32_t t_type = 0;
        file.read(reinterpret_cast<char*>(&t_type), 4);
        quant_counts[t_type]++;
        if (!native_tensor_supported(tensor_name, static_cast<TensorType>(t_type)))
            res.unsupported_native_tensors.push_back(tensor_name + ": " + quant_type_to_string(t_type));
        uint64_t offset = 0;
        file.read(reinterpret_cast<char*>(&offset), 8);
    }

    res.param_count = total_params;
    res.param_size_str = format_param_count(total_params);

    uint32_t dominant_type = 8; // default Q8_0
    uint64_t max_count = 0;
    for (const auto& pair : quant_counts) {
        if (pair.second > max_count) {
            max_count = pair.second;
            dominant_type = pair.first;
        }
    }
    res.quant_type = quant_type_to_string(dominant_type);

    if (res.architecture.empty()) {
        std::string lower = std::filesystem::path(file_path).filename().string();
        for (char& c : lower) c = ::tolower(c);
        if (lower.find("diffusion") != std::string::npos || lower.find("sd") != std::string::npos) {
            res.architecture = "diffusion";
        }
    }

    uint64_t max_allowed_params = 1850000000ULL;
    if (res.architecture == "diffusion" || res.architecture == "unet") {
        max_allowed_params = 4000000000ULL;
    }

    if (total_params > max_allowed_params) {
        res.is_valid = false;
        res.error_message = "Parameter count (" + res.param_size_str + ") exceeds maximum limit.";
        return res;
    }

    const std::vector<std::string> all_roles = {"general", "coder", "compressor", "router", "embedding", "audio_stt", "image_gen"};
    for (const auto& r : all_roles) {
        if (is_role_compatible(res.architecture, r)) {
            res.compatible_roles.push_back(r);
        }
    }

    res.native_tensor_compatible = file.good() && tensor_count > 0 && res.unsupported_native_tensors.empty();
    res.is_valid = file.good();
    return res;
}
