#include "tokenizer.hpp"
#include <iostream>

Tokenizer::Tokenizer()
    : vocab_(nullptr), eos_token_id_(-1), bos_token_id_(-1) {}

Tokenizer::Tokenizer(const Vocab* vocab, int eos_token_id, int bos_token_id)
    : vocab_(vocab), eos_token_id_(eos_token_id), bos_token_id_(bos_token_id) {}

std::vector<int> Tokenizer::encode(const std::string& text) const {
    std::vector<int> tokens;
    if (!vocab_ || !vocab_->root) return tokens;

    std::string bpe_text;
    bpe_text.reserve(text.size() * 2);
    for (char c : text) {
        if (c == ' ') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0xA0;
        } else if (c == '\n') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0x8A;
        } else {
            bpe_text += c;
        }
    }

    size_t i = 0;
    while (i < bpe_text.length()) {
        int best_id = -1;
        size_t best_len = 0;

        TrieNode* curr = vocab_->root.get();
        for (size_t j = i; j < bpe_text.length(); ++j) {
            char c = bpe_text[j];
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                break;
            }
            curr = it->second.get();
            if (curr->token_id != -1) {
                best_id = curr->token_id;
                best_len = j - i + 1;
            }
        }

        if (best_id == -1) {
            // Unmatched single character fallback
            i += 1;
        } else {
            tokens.push_back(best_id);
            i += best_len;
        }
    }
    return tokens;
}

std::string Tokenizer::decode(int token_id) const {
    if (!vocab_) return "";
    if (token_id >= 0 && token_id < (int)vocab_->tokens.size()) {
        std::string s = vocab_->tokens[token_id];

        // Replace all instances of Ġ (0xC4 0xA0) with space
        size_t pos = 0;
        while ((pos = s.find("\xC4\xA0", pos)) != std::string::npos) {
            s.replace(pos, 2, " ");
            pos += 1;
        }

        // Replace all instances of Ċ (0xC4 0x8A) with newline
        pos = 0;
        while ((pos = s.find("\xC4\x8A", pos)) != std::string::npos) {
            s.replace(pos, 2, "\n");
            pos += 1;
        }

        return s;
    }
    return "";
}

std::string Tokenizer::decode(const std::vector<int>& tokens) const {
    std::string result;
    for (int t : tokens) {
        result += decode(t);
    }
    return result;
}

size_t Tokenizer::count_tokens(const std::string& text) const {
    if (!vocab_ || !vocab_->root) {
        // Fallback approximation only if no vocab loaded
        return (text.size() + 3) / 4;
    }

    std::string bpe_text;
    bpe_text.reserve(text.size() * 2);
    for (char c : text) {
        if (c == ' ') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0xA0;
        } else if (c == '\n') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0x8A;
        } else {
            bpe_text += c;
        }
    }

    size_t count = 0;
    size_t i = 0;
    while (i < bpe_text.length()) {
        int best_id = -1;
        size_t best_len = 0;

        TrieNode* curr = vocab_->root.get();
        for (size_t j = i; j < bpe_text.length(); ++j) {
            char c = bpe_text[j];
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                break;
            }
            curr = it->second.get();
            if (curr->token_id != -1) {
                best_id = curr->token_id;
                best_len = j - i + 1;
            }
        }

        if (best_id == -1) {
            i += 1;
        } else {
            count++;
            i += best_len;
        }
    }
    return count;
}
