#include "tokenizer_registry.hpp"

void TokenizerRegistry::register_tokenizer(const std::string& model_id, const Vocab* vocab, int eos_token_id, int bos_token_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    tokenizers_[model_id] = std::make_unique<Tokenizer>(vocab, eos_token_id, bos_token_id);
    if (default_model_id_.empty() || model_id == "qwen_main") {
        default_model_id_ = model_id;
    }
}

const Tokenizer* TokenizerRegistry::get_tokenizer(const std::string& model_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokenizers_.find(model_id);
    if (it != tokenizers_.end()) {
        return it->second.get();
    }
    if (!default_model_id_.empty()) {
        auto def_it = tokenizers_.find(default_model_id_);
        if (def_it != tokenizers_.end()) {
            return def_it->second.get();
        }
    }
    return nullptr;
}

const Tokenizer* TokenizerRegistry::get_default_tokenizer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!default_model_id_.empty()) {
        auto it = tokenizers_.find(default_model_id_);
        if (it != tokenizers_.end()) {
            return it->second.get();
        }
    }
    if (!tokenizers_.empty()) {
        return tokenizers_.begin()->second.get();
    }
    return nullptr;
}

size_t TokenizerRegistry::count_tokens(const std::string& model_id, const std::string& text) const {
    const Tokenizer* tok = get_tokenizer(model_id);
    if (tok) {
        return tok->count_tokens(text);
    }
    // Fallback: 4 chars per token if no tokenizer registered
    return (text.size() + 3) / 4;
}
