#pragma once
#include "tokenizer.hpp"
#include "model_registry.hpp"
#include <map>
#include <string>
#include <memory>
#include <mutex>

class TokenizerRegistry {
public:
    TokenizerRegistry() = default;

    void register_tokenizer(const std::string& model_id, const Vocab* vocab, int eos_token_id = -1, int bos_token_id = -1);
    const Tokenizer* get_tokenizer(const std::string& model_id) const;
    const Tokenizer* get_default_tokenizer() const;

    size_t count_tokens(const std::string& model_id, const std::string& text) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Tokenizer>> tokenizers_;
    std::string default_model_id_;
};
