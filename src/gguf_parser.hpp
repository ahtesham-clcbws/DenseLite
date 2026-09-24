#pragma once

#include "model.hpp"
#include <string>

// Loads a GGUF file from disk using mmap and parses the tensor metadata.
// Enforces 32-byte alignment for all tensor data offsets.
bool load_gguf_model(const std::string& file_path, DenseModel& out_model);

// Unmaps the file and cleans up resources.
void free_gguf_model(DenseModel& model);
