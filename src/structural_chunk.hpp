#pragma once
#include <string>
#include <cstdint>

struct StructuralChunk {
    std::string file_path;
    std::string symbol;        // e.g. "AuthController::login"
    std::string parent_symbol; // e.g. "AuthController"
    std::string language;
    int start_line = 0;
    int end_line = 0;
    uint64_t source_hash = 0;
    std::string content;
};
