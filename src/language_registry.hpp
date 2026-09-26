#pragma once
#include <string>
#include <map>

// Forward declaration of Tree-sitter Language struct
typedef struct TSLanguage TSLanguage;

class LanguageRegistry {
public:
    LanguageRegistry();

    // Resolves language name from file extension
    std::string detect_language(const std::string& file_path) const;

    // Returns the Tree-sitter grammar function pointer for a language
    const TSLanguage* get_grammar_for_language(const std::string& language) const;
    const TSLanguage* get_grammar_for_file(const std::string& file_path) const;

    bool is_supported(const std::string& file_path) const;

private:
    std::map<std::string, std::string> extension_map_;
};
