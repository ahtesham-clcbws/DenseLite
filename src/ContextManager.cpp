#include "ContextManager.hpp"
#include "context_compressor.hpp"
#include "infer.hpp"
#include "vector_search.hpp"
#include <iostream>
#include <algorithm>

void ContextManager::optimize_context(OpenAIRequest& req, int max_context_tokens, bool use_semantic_selection, DenseModel* nomic) {
    if (req.messages.empty()) return;
    
    if (!nomic) nomic = VectorSearch::get_embedding_model();
    if (use_semantic_selection && nomic != nullptr) {
        req.messages = semantic_filter(req.messages, nomic);
    } else {
        req.messages = truncate_history(req.messages, max_context_tokens);
    }
}

std::vector<OpenAIMessage> ContextManager::truncate_history(const std::vector<OpenAIMessage>& messages, int max_context_tokens) {
    auto deduplicated = ContextCompressor::deduplicate(messages);
    return ContextCompressor::sliding_window(deduplicated, nullptr, static_cast<size_t>(max_context_tokens));
}

std::vector<OpenAIMessage> ContextManager::semantic_filter(const std::vector<OpenAIMessage>& messages, DenseModel* nomic) {
    if (messages.size() < 2) return messages;
    if (!nomic) nomic = VectorSearch::get_embedding_model();
    if (!nomic) return truncate_history(messages, 8192);

    const auto& last_msg = messages.back();
    auto query_tokens = tokenize(nomic->vocab, last_msg.content);
    auto query_vec = compute_embedding(*nomic, query_tokens);
    if (query_vec.empty()) return truncate_history(messages, 8192);

    std::vector<OpenAIMessage> preserved;
    size_t start_idx = 0;
    if (messages.front().role == "system") {
        preserved.push_back(messages.front());
        start_idx = 1;
    }

    // Score intermediate turns against the active query embedding
    std::vector<std::pair<float, OpenAIMessage>> scored;
    for (size_t i = start_idx; i < messages.size() - 1; ++i) {
        auto msg_tokens = tokenize(nomic->vocab, messages[i].content);
        auto msg_vec = compute_embedding(*nomic, msg_tokens);
        float sim = VectorSearch::cosine_similarity(query_vec, msg_vec);
        scored.emplace_back(sim, messages[i]);
    }

    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    for (const auto& item : scored) {
        preserved.push_back(item.second);
    }
    preserved.push_back(last_msg);
    return truncate_history(preserved, 8192);
}

