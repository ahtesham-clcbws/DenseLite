#pragma once
#include <string>
#include <map>
#include <vector>
#include "httplib.h"
#include "model.hpp"
#include "sqlite_router.hpp"
#include "Curator.hpp"
#include "RequestAnalyzer.hpp"
#include "context_engine.hpp"
#include "tokenizer_registry.hpp"
#include "memory_engine.hpp"
#include "code_indexer.hpp"
#include "search_engine.hpp"

struct InferenceSession {
    std::string session_id;
    std::string request_id;
    SessionStatus status;
    std::string task_type;
    std::string model;
    int iteration_count = 0;
    std::vector<std::string> iteration_results;
    bool waiting_for_tool = false;
};

class DenseLiteEngine {
public:
    DenseLiteEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router);
    
    // Process the incoming generation request
    void process(const std::string& request_body, httplib::Response& res);

    TokenizerRegistry& get_tokenizer_registry() { return tokenizer_registry_; }
    ContextEngine& get_context_engine() { return context_engine_; }
    MemoryEngine& get_memory_engine() { return memory_engine_; }
    CodeIndexer& get_code_indexer() { return code_indexer_; }
    SearchEngine& get_search_engine() { return search_engine_; }

private:
    std::map<std::string, DenseModel>& models;
    SQLiteRouter& router;
    TokenizerRegistry tokenizer_registry_;
    ContextEngine context_engine_;
    MemoryEngine memory_engine_;
    CodeIndexer code_indexer_;
    SearchEngine search_engine_;

    InferenceSession get_or_create_session(const std::string& session_id);
    void execute_pipeline(InferenceSession& session, OpenAIRequest& parsed_req, httplib::Response& res);
};
