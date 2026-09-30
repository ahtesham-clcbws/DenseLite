#include "search_engine.hpp"
#include <algorithm>

SearchEngine::SearchEngine(CodeIndexer* code_indexer, MemoryEngine* memory_engine)
    : code_indexer_(code_indexer), memory_engine_(memory_engine) {}

void SearchEngine::set_code_indexer(CodeIndexer* indexer) {
    std::lock_guard<std::mutex> lock(mutex_);
    code_indexer_ = indexer;
}

void SearchEngine::set_memory_engine(MemoryEngine* engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_engine_ = engine;
}

void SearchEngine::set_vector_index(TurboQuantIndex* index) {
    std::lock_guard<std::mutex> lock(mutex_);
    vector_index_ = index;
}

void SearchEngine::add_custom_chunk(const StructuralChunk& chunk) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_chunks_.push_back(chunk);
}

void SearchEngine::clear_custom_chunks() {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_chunks_.clear();
}

std::vector<SearchResult> SearchEngine::search(const std::string& query,
                                             const std::string& active_task,
                                             size_t top_k) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SearchResult> all_candidates;

    // 1. Gather all available code chunks
    std::vector<StructuralChunk> code_chunks = custom_chunks_;
    if (code_indexer_) {
        auto indexed = code_indexer_->symbols().find_by_symbol(query);
        code_chunks.insert(code_chunks.end(), indexed.begin(), indexed.end());
    }

    // 2. Gather memory entries
    std::vector<std::pair<std::string, std::string>> memory_texts;
    if (memory_engine_) {
        MemoryScopeFilter scope_filter;
        scope_filter.workspace_id = workspace_id_;
        scope_filter.active_only = true;
        for (const auto& mem : memory_engine_->store().query_memories_keyword(query, 20, scope_filter)) {
            memory_texts.emplace_back(mem.key, mem.value);
        }
    }

    // 3. Multi-channel search
    // Channel A: ExactSearch
    auto exact_code = ExactSearch::search_code(query, code_chunks);
    all_candidates.insert(all_candidates.end(), exact_code.begin(), exact_code.end());

    // Channel B: LexicalSearch
    auto lexical_code = LexicalSearch::search_chunks(query, code_chunks);
    all_candidates.insert(all_candidates.end(), lexical_code.begin(), lexical_code.end());
    auto lexical_mem = LexicalSearch::search_texts(query, memory_texts);
    for (auto& item : lexical_mem) {
        item.source = SearchSource::MEMORY;
        all_candidates.push_back(item);
    }

    // Channel C: StructuralSearch
    auto struct_code = StructuralSearch::search_all(query, code_chunks);
    all_candidates.insert(all_candidates.end(), struct_code.begin(), struct_code.end());

    // Channel D: VectorSearch (Semantic Vector Index & RAG Fusion - IDs 89-90)
    auto query_vec = VectorSearch::embed_text(query);
    if (!query_vec.empty()) {
        if (memory_engine_) {
            MemoryScopeFilter scope_filter;
            scope_filter.workspace_id = workspace_id_;
            scope_filter.active_only = true;
            auto vec_mem_hits = memory_engine_->query_memories_vector(query_vec, top_k * 2, 0.15f, scope_filter);
            for (const auto& hit : vec_mem_hits) {
                SearchResult r;
                r.id = hit.first.id.empty() ? hit.first.key : hit.first.id;
                r.source = SearchSource::MEMORY;
                r.content = hit.first.value;
                r.semantic_similarity = hit.second;
                r.timestamp = hit.first.updated_at;
                all_candidates.push_back(std::move(r));
            }
        }

        if (vector_index_) {
            auto tq_hits = vector_index_->search(query_vec, top_k);
            for (const auto& hit : tq_hits) {
                SearchResult r;
                r.id = hit.id;
                r.source = SearchSource::CODE;
                r.semantic_similarity = hit.score;
                
                if (code_indexer_) {
                    auto indexed = code_indexer_->symbols().find_by_symbol(hit.id);
                    if (!indexed.empty()) {
                        r.content = indexed[0].content;
                        r.file_path = indexed[0].file_path;
                        r.symbol_name = indexed[0].symbol;
                    } else {
                        r.content = "Vector Match: " + hit.id;
                        r.symbol_name = hit.id;
                    }
                } else {
                    r.content = "Vector Match: " + hit.id;
                    r.symbol_name = hit.id;
                }
                all_candidates.push_back(std::move(r));
            }
        }
    }

    // 4. Assign task relevance if active_task is provided
    if (!active_task.empty()) {
        auto task_terms = LexicalSearch::tokenize(active_task);
        for (auto& c : all_candidates) {
            c.task_relevance = LexicalSearch::score_content(task_terms, c.content);
        }
    }

    // 5. ResultFusion: deduplicate and rank
    return ResultFusion::fuse(all_candidates, top_k);
}

std::vector<SearchResult> SearchEngine::search_code(const std::string& query, size_t top_k) {
    return search(query, "", top_k);
}

std::vector<SearchResult> SearchEngine::search_memory(const std::string& query, size_t top_k) {
    auto results = search(query, "", top_k * 2);
    std::vector<SearchResult> mem_only;
    for (const auto& r : results) {
        if (r.source == SearchSource::MEMORY) {
            mem_only.push_back(r);
            if (mem_only.size() >= top_k) break;
        }
    }
    return mem_only;
}
