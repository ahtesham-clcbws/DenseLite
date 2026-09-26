#include "language_registry.hpp"
#include <algorithm>

extern "C" const TSLanguage *tree_sitter_cpp();
extern "C" const TSLanguage *tree_sitter_python();
extern "C" const TSLanguage *tree_sitter_php();
extern "C" const TSLanguage *tree_sitter_javascript();

LanguageRegistry::LanguageRegistry() {
    extension_map_[".cpp"] = "cpp";
    extension_map_[".hpp"] = "cpp";
    extension_map_[".cc"] = "cpp";
    extension_map_[".cxx"] = "cpp";
    extension_map_[".c"] = "cpp";
    extension_map_[".h"] = "cpp";

    extension_map_[".py"] = "python";
    extension_map_[".pyi"] = "python";

    extension_map_[".php"] = "php";

    extension_map_[".js"] = "javascript";
    extension_map_[".jsx"] = "javascript";
    extension_map_[".ts"] = "javascript";
    extension_map_[".tsx"] = "javascript";
    extension_map_[".mjs"] = "javascript";
}

std::string LanguageRegistry::detect_language(const std::string& file_path) const {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos == std::string::npos) return "";

    std::string ext = file_path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = extension_map_.find(ext);
    if (it != extension_map_.end()) {
        return it->second;
    }
    return "";
}

const TSLanguage* LanguageRegistry::get_grammar_for_language(const std::string& language) const {
    if (language == "cpp") return tree_sitter_cpp();
    if (language == "python") return tree_sitter_python();
    if (language == "php") return tree_sitter_php();
    if (language == "javascript") return tree_sitter_javascript();
    return nullptr;
}

const TSLanguage* LanguageRegistry::get_grammar_for_file(const std::string& file_path) const {
    std::string lang = detect_language(file_path);
    if (lang.empty()) return nullptr;
    return get_grammar_for_language(lang);
}

bool LanguageRegistry::is_supported(const std::string& file_path) const {
    return !detect_language(file_path).empty();
}
