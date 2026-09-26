#pragma once
#include "structural_chunk.hpp"
#include "language_registry.hpp"
#include <string>
#include <vector>

class ASTChunker {
public:
    explicit ASTChunker(LanguageRegistry* registry = nullptr);

    // Chunks source code into structural AST definitions (functions, classes, methods)
    std::vector<StructuralChunk> chunk(const std::string& file_path,
                                       const std::string& source_code);

private:
    LanguageRegistry* registry_ = nullptr;
    LanguageRegistry default_registry_;
};
