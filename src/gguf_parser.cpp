#include "gguf_parser.hpp"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <functional>

// GGUF Magic Bytes: "GGUF" (0x46554747)
const uint32_t GGUF_MAGIC = 0x46554747;

bool load_gguf_model(const std::string& file_path, DenseModel& out_model) {
    std::cout << "[GGUF] Attempting to load: " << file_path << std::endl;

    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[GGUF] Error: Failed to open file." << std::endl;
        return false;
    }

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        std::cerr << "[GGUF] Error: Failed to get file stats." << std::endl;
        close(fd);
        return false;
    }
    out_model.mmap_size = sb.st_size;

    // Map the file into memory (MAP_SHARED for zero-copy read-only access)
    out_model.mmap_data = mmap(nullptr, out_model.mmap_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd); // safe to close fd after mmap

    if (out_model.mmap_data == MAP_FAILED) {
        std::cerr << "[GGUF] Error: mmap failed." << std::endl;
        return false;
    }

    uint8_t* ptr = static_cast<uint8_t*>(out_model.mmap_data);

    // 1. Validate Magic Bytes
    uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(magic));
    if (magic != GGUF_MAGIC) {
        std::cerr << "[GGUF] Error: Invalid Magic Bytes. Not a GGUF file." << std::endl;
        return false;
    }
    ptr += sizeof(magic);
    std::cout << "[GGUF] Magic bytes validated successfully." << std::endl;

    // 2. Validate Version
    uint32_t version;
    std::memcpy(&version, ptr, sizeof(version));
    ptr += sizeof(version);
    std::cout << "[GGUF] Version: v" << version << std::endl;

    // 3. Tensor and KV Counts
    uint64_t tensor_count;
    std::memcpy(&tensor_count, ptr, sizeof(tensor_count));
    ptr += sizeof(tensor_count);
    
    uint64_t kv_count;
    std::memcpy(&kv_count, ptr, sizeof(kv_count));
    ptr += sizeof(kv_count);

    std::cout << "[GGUF] Tensor Count: " << tensor_count << ", KV Count: " << kv_count << std::endl;

    // Helper to read strings
    auto read_string = [&ptr]() -> std::string {
        uint64_t len;
        std::memcpy(&len, ptr, sizeof(len));
        ptr += sizeof(len);
        std::string str(reinterpret_cast<char*>(ptr), len);
        ptr += len;
        return str;
    };

    // Helper to skip a value based on gguf_type
    std::function<void(uint32_t)> skip_value = [&ptr, &skip_value, &read_string](uint32_t type) {
        switch (type) {
            case 0: case 1: case 7: ptr += 1; break; // UINT8, INT8, BOOL
            case 2: case 3: ptr += 2; break;         // UINT16, INT16
            case 4: case 5: case 6: ptr += 4; break; // UINT32, INT32, FLOAT32
            case 10: case 11: case 12: ptr += 8; break; // UINT64, INT64, FLOAT64
            case 8: read_string(); break; // STRING
            case 9: { // ARRAY
                uint32_t arr_type;
                std::memcpy(&arr_type, ptr, sizeof(arr_type));
                ptr += sizeof(arr_type);
                uint64_t arr_len;
                std::memcpy(&arr_len, ptr, sizeof(arr_len));
                ptr += sizeof(arr_len);
                
                // Recursively skip each element in the array
                for (uint64_t j = 0; j < arr_len; ++j) {
                    skip_value(arr_type);
                }
                break;
            }
            default: std::cerr << "Unknown GGUF type: " << type << std::endl; ptr += 4;
        }
    };

    // 4. Parse KV pairs
    out_model.config.alignment = 32; // Default GGUF alignment
    for (uint64_t i = 0; i < kv_count; ++i) {
        std::string key = read_string();
        uint32_t type;
        std::memcpy(&type, ptr, sizeof(type));
        ptr += sizeof(type);

        if (key == "general.alignment" && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.alignment, ptr, sizeof(uint32_t));
        } else if (key == "tokenizer.ggml.tokens" && type == 9 /* ARRAY */) {
            uint32_t arr_type;
            std::memcpy(&arr_type, ptr, sizeof(arr_type));
            ptr += sizeof(arr_type);
            uint64_t arr_len;
            std::memcpy(&arr_len, ptr, sizeof(arr_len));
            ptr += sizeof(arr_len);
            
            std::cout << "[GGUF] Loading vocab: " << arr_len << " tokens." << std::endl;
            out_model.vocab.tokens.reserve(arr_len);
            for (uint64_t j = 0; j < arr_len; ++j) {
                if (arr_type == 8 /* STRING */) {
                    std::string t_str = read_string();
                    out_model.vocab.tokens.push_back(t_str);
                    
                    // Add to trie
                    TrieNode* curr = out_model.vocab.root.get();
                    for (char c : t_str) {
                        if (!curr->children[c]) {
                            curr->children[c] = std::make_unique<TrieNode>();
                        }
                        curr = curr->children[c].get();
                    }
                    curr->token_id = j;
                } else {
                    skip_value(arr_type);
                }
            }
            out_model.config.vocab_size = arr_len;
        } else if (key == "tokenizer.ggml.scores" && type == 9 /* ARRAY */) {
            uint32_t arr_type;
            std::memcpy(&arr_type, ptr, sizeof(arr_type));
            ptr += sizeof(arr_type);
            uint64_t arr_len;
            std::memcpy(&arr_len, ptr, sizeof(arr_len));
            ptr += sizeof(arr_len);
            
            out_model.vocab.scores.reserve(arr_len);
            for (uint64_t j = 0; j < arr_len; ++j) {
                if (arr_type == 6 /* FLOAT32 */) {
                    float score;
                    std::memcpy(&score, ptr, sizeof(score));
                    ptr += sizeof(score);
                    out_model.vocab.scores.push_back(score);
                } else {
                    skip_value(arr_type);
                }
            }
        } else if (key.find(".context_length") != std::string::npos && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.context_length, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
        } else if (key.find(".embedding_length") != std::string::npos && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.embedding_length, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
        } else if (key.find(".block_count") != std::string::npos && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.num_layers, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
        } else if (key.find(".attention.head_count") != std::string::npos && key.find(".attention.head_count_kv") == std::string::npos && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.num_heads, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
        } else if (key.find(".attention.head_count_kv") != std::string::npos && type == 4 /* UINT32 */) {
            std::memcpy(&out_model.config.num_kv_heads, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
        } else if (key.find(".attention.layer_norm_rms_epsilon") != std::string::npos && type == 6 /* FLOAT32 */) {
            std::memcpy(&out_model.config.rms_norm_eps, ptr, sizeof(float));
            ptr += sizeof(float);
        } else {
            // Skip unhandled metadata values
            skip_value(type);
        }
    }
    
    
    // Compute derived dims
    if (out_model.config.num_heads > 0) {
        out_model.config.head_dim = out_model.config.embedding_length / out_model.config.num_heads;
    }
    
    // 5. Parse Tensors
    for (uint64_t i = 0; i < tensor_count; ++i) {
        Tensor t;
        t.name = read_string();
        
        uint32_t n_dims;
        std::memcpy(&n_dims, ptr, sizeof(n_dims));
        ptr += sizeof(n_dims);
        
        for (uint32_t d = 0; d < n_dims; ++d) {
            uint64_t dim_size;
            std::memcpy(&dim_size, ptr, sizeof(dim_size));
            ptr += sizeof(dim_size);
            t.shape.push_back((uint32_t)dim_size);
        }
        
        uint32_t t_type;
        std::memcpy(&t_type, ptr, sizeof(t_type));
        ptr += sizeof(t_type);
        t.type = static_cast<TensorType>(t_type);
        
        uint64_t offset;
        std::memcpy(&offset, ptr, sizeof(offset));
        ptr += sizeof(offset);
        
        t.data_offset = offset;
        out_model.tensors[t.name] = t;
    }
    
    // Compute the start of tensor data
    // The data starts immediately after the tensor metadata, padded to 'alignment'
    size_t metadata_size = ptr - static_cast<uint8_t*>(out_model.mmap_data);
    size_t padding = (out_model.config.alignment - (metadata_size % out_model.config.alignment)) % out_model.config.alignment;
    size_t tensor_data_start = metadata_size + padding;
    
    // Assign pointers and validate alignment
    for (auto& pair : out_model.tensors) {
        Tensor& t = pair.second;
        size_t absolute_offset = tensor_data_start + t.data_offset;
        
        if (absolute_offset % 32 != 0) {
            std::cerr << "[GGUF] FATAL: Tensor '" << t.name << "' offset (" << absolute_offset 
                      << ") is NOT 32-byte aligned! AVX2 will segfault." << std::endl;
            return false;
        }
        
        t.data = static_cast<uint8_t*>(out_model.mmap_data) + absolute_offset;
    }

    // Example tensor alignment validation logic:
    // When calculating the offset for tensor data, we enforce modulo 32.
    // size_t tensor_offset = ...;
    // if (tensor_offset % out_model.config.alignment != 0) {
    //     std::cerr << "[GGUF] FATAL: Tensor offset misaligned! AVX2 will segfault." << std::endl;
    //     return false;
    // }

    std::cout << "[GGUF] Model mapped successfully. Alignment strict check: " 
              << out_model.config.alignment << "-byte boundaries." << std::endl;

    return true;
}

void free_gguf_model(DenseModel& model) {
    if (model.mmap_data && model.mmap_data != MAP_FAILED) {
        munmap(model.mmap_data, model.mmap_size);
        model.mmap_data = nullptr;
        model.mmap_size = 0;
        std::cout << "[GGUF] Model unmapped from memory." << std::endl;
    }
}
