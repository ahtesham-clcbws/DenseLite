#pragma once
#include "model.hpp"
#include <string>
#include <vector>
#include <memory>

class Tokenizer {
public:
    Tokenizer();
    explicit Tokenizer(const Vocab* vocab, int eos_token_id = -1, int bos_token_id = -1);

    bool is_valid() const { return vocab_ != nullptr; }

    // Tokenization & Detokenization
    std::vector<int> encode(const std::string& text) const;
    std::string decode(int token_id) const;
    std::string decode(const std::vector<int>& tokens) const;

    // Fast token counting without building the full token vector
    size_t count_tokens(const std::string& text) const;

    int eos_token_id() const { return eos_token_id_; }
    int bos_token_id() const { return bos_token_id_; }
    size_t vocab_size() const { return vocab_ ? vocab_->tokens.size() : 0; }

private:
    const Vocab* vocab_ = nullptr;
    int eos_token_id_ = -1;
    int bos_token_id_ = -1;
};
