#include "ast_chunker.hpp"
#include <tree_sitter/api.h>
#include <functional>
#include <sstream>

ASTChunker::ASTChunker(LanguageRegistry* registry)
    : registry_(registry ? registry : &default_registry_) {}

static std::string extract_node_text(TSNode node, const std::string& src) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start < src.size() && end <= src.size() && start <= end) {
        return src.substr(start, end - start);
    }
    return "";
}

static std::string find_identifier_name(TSNode node, const std::string& src) {
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(node, i);
        std::string type = ts_node_type(child);
        if (type == "identifier" || type == "name" || type == "type_identifier" || type == "field_identifier") {
            return extract_node_text(child, src);
        }
        if (type == "function_declarator" || type == "declarator") {
            std::string sub = find_identifier_name(child, src);
            if (!sub.empty()) return sub;
        }
    }
    return "";
}

static bool is_structural_definition(const std::string& type) {
    return (type == "function_definition" ||
            type == "class_definition" ||
            type == "class_specifier" ||
            type == "class_declaration" ||
            type == "struct_specifier" ||
            type == "method_declaration" ||
            type == "method_definition" ||
            type == "function_declaration");
}

static void walk_ast(TSNode node,
                     const std::string& file_path,
                     const std::string& language,
                     const std::string& src,
                     const std::string& parent_sym,
                     std::vector<StructuralChunk>& out) {

    std::string type = ts_node_type(node);
    std::string current_parent = parent_sym;

    if (is_structural_definition(type)) {
        std::string sym = find_identifier_name(node, src);
        if (sym.empty()) {
            sym = "<anonymous_" + type + ">";
        }
        std::string full_sym = parent_sym.empty() ? sym : parent_sym + "::" + sym;

        TSPoint start = ts_node_start_point(node);
        TSPoint end = ts_node_end_point(node);

        StructuralChunk chunk;
        chunk.file_path = file_path;
        chunk.symbol = full_sym;
        chunk.parent_symbol = parent_sym;
        chunk.language = language;
        chunk.start_line = start.row + 1;
        chunk.end_line = end.row + 1;
        chunk.content = extract_node_text(node, src);
        chunk.source_hash = std::hash<std::string>{}(chunk.content);

        out.push_back(std::move(chunk));
        current_parent = full_sym;
    }

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        walk_ast(ts_node_child(node, i), file_path, language, src, current_parent, out);
    }
}

std::vector<StructuralChunk> ASTChunker::chunk(const std::string& file_path,
                                              const std::string& source_code) {
    std::vector<StructuralChunk> chunks;
    if (source_code.empty()) return chunks;

    std::string lang = registry_->detect_language(file_path);
    const TSLanguage* grammar = registry_->get_grammar_for_language(lang);
    if (!grammar) {
        // Fallback: entire file as one module chunk
        StructuralChunk c;
        c.file_path = file_path;
        c.symbol = "<module>";
        c.language = "raw";
        c.start_line = 1;
        c.end_line = 1;
        c.content = source_code;
        c.source_hash = std::hash<std::string>{}(source_code);
        chunks.push_back(std::move(c));
        return chunks;
    }

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, grammar);

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source_code.data(), static_cast<uint32_t>(source_code.size()));
    if (tree) {
        TSNode root = ts_tree_root_node(tree);
        walk_ast(root, file_path, lang, source_code, "", chunks);
        ts_tree_delete(tree);
    }
    ts_parser_delete(parser);

    if (chunks.empty()) {
        StructuralChunk c;
        c.file_path = file_path;
        c.symbol = "<module>";
        c.language = lang;
        c.start_line = 1;
        c.end_line = 1;
        c.content = source_code;
        c.source_hash = std::hash<std::string>{}(source_code);
        chunks.push_back(std::move(c));
    }

    return chunks;
}
