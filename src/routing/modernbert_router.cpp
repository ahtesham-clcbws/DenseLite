#include "modernbert_router.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <onnxruntime_cxx_api.h>
#include "model.hpp"
#include "tokenizer.hpp"
#include "../settings/path_service.hpp"
#include "../../dependencies/json.hpp"

using json = nlohmann::json;

struct Candidate {
    std::string label;
    std::vector<int64_t> hyp_tokens;
};

struct ModernBERTRouter::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "ModernBERTRouter"};
    Ort::SessionOptions session_opts;
    std::unique_ptr<Ort::Session> session;
    Vocab vocab;
    std::unique_ptr<Tokenizer> tokenizer;
    std::vector<Candidate> candidates;
    std::mutex mtx;
    std::atomic<bool> ready{false};

    Impl() {
        session_opts.SetIntraOpNumThreads(2);
        session_opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        candidates = {
            {"coding", {1552, 2505, 310, 670, 4382, 10717, 285, 12425, 15, 50282}},
            {"image", {1552, 2505, 310, 670, 2460, 5978, 13, 10263, 13, 390, 5304, 1445, 15, 50282}},
            {"audio", {1552, 2505, 310, 670, 6519, 13, 9797, 9464, 13, 390, 3590, 7663, 15, 50282}},
            {"reasoning", {1552, 2505, 310, 2087, 14720, 13, 2892, 13, 390, 7827, 15, 50282}},
            {"compressor", {1552, 2505, 310, 24433, 2505, 10405, 1320, 390, 13800, 15, 50282}}
        };
    }
};

ModernBERTRouter::ModernBERTRouter() : impl_(std::make_unique<Impl>()) {}
ModernBERTRouter::~ModernBERTRouter() = default;

ModernBERTRouter& ModernBERTRouter::instance() {
    static ModernBERTRouter inst;
    return inst;
}

bool ModernBERTRouter::initialize(const std::string& model_dir) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (impl_->ready) return true;

    std::string effective_dir = model_dir;
    std::string tok_path = effective_dir + "/tokenizer.json";
    if (!std::filesystem::exists(tok_path)) {
        std::string ps_dir = PathService::instance().get_models_dir() + "/modernbert";
        if (std::filesystem::exists(ps_dir + "/tokenizer.json")) {
            effective_dir = ps_dir;
            tok_path = effective_dir + "/tokenizer.json";
        } else if (std::filesystem::exists("../models/modernbert/tokenizer.json")) {
            effective_dir = "../models/modernbert";
            tok_path = effective_dir + "/tokenizer.json";
        }
    }

    std::string onnx_path = effective_dir + "/model.onnx";

    std::ifstream tok_file(tok_path);
    if (!tok_file.is_open()) {
        std::cerr << "[ModernBERTRouter] Tokenizer file not found: " << tok_path << std::endl;
        return false;
    }

    try {
        json j = json::parse(tok_file);
        auto& v = j["model"]["vocab"];
        impl_->vocab.tokens.resize(50285);
        for (auto it = v.begin(); it != v.end(); ++it) {
            int id = it.value().get<int>();
            if (id >= 0 && id < (int)impl_->vocab.tokens.size()) {
                impl_->vocab.tokens[id] = it.key();
            }
        }
        for (size_t id = 0; id < impl_->vocab.tokens.size(); ++id) {
            const std::string& s = impl_->vocab.tokens[id];
            if (s.empty()) continue;
            TrieNode* curr = impl_->vocab.root.get();
            for (char c : s) {
                if (curr->children.find(c) == curr->children.end()) {
                    curr->children[c] = std::make_unique<TrieNode>();
                }
                curr = curr->children[c].get();
            }
            curr->token_id = (int)id;
        }
        impl_->tokenizer = std::make_unique<Tokenizer>(&impl_->vocab, 50282, 50281);
        impl_->session = std::make_unique<Ort::Session>(impl_->env, onnx_path.c_str(), impl_->session_opts);
        impl_->ready = true;
        std::cout << "[ModernBERTRouter] Initialized internal ModernBERT router from " << model_dir << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ModernBERTRouter] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool ModernBERTRouter::is_available() const {
    return impl_->ready;
}

std::vector<ModernBERTScore> ModernBERTRouter::rank_intents(const std::string& user_query) {
    if (!impl_->ready && !initialize()) return {};
    std::lock_guard<std::mutex> lock(impl_->mtx);

    auto query_tokens = impl_->tokenizer->encode(user_query);
    if (query_tokens.size() > 450) query_tokens.resize(450);

    std::vector<ModernBERTScore> scores;
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    for (const auto& c : impl_->candidates) {
        std::vector<int64_t> input_ids;
        input_ids.reserve(query_tokens.size() + c.hyp_tokens.size() + 2);
        input_ids.push_back(50281); // [CLS]
        for (int t : query_tokens) input_ids.push_back(t);
        input_ids.push_back(50282); // [SEP]
        for (int64_t t : c.hyp_tokens) input_ids.push_back(t);

        std::vector<int64_t> attention_mask(input_ids.size(), 1);
        std::vector<int64_t> shape = {1, (int64_t)input_ids.size()};

        Ort::Value input_tensors[2] = {
            Ort::Value::CreateTensor<int64_t>(mem_info, input_ids.data(), input_ids.size(), shape.data(), shape.size()),
            Ort::Value::CreateTensor<int64_t>(mem_info, attention_mask.data(), attention_mask.size(), shape.data(), shape.size())
        };

        const char* input_names[] = {"input_ids", "attention_mask"};
        const char* output_names[] = {"logits"};

        auto out = impl_->session->Run(Ort::RunOptions{nullptr}, input_names, input_tensors, 2, output_names, 1);
        float* logits = out[0].GetTensorMutableData<float>();
        float l_ent = logits[0];
        float l_not = logits[1];
        float prob = 1.0f / (1.0f + std::exp(l_not - l_ent));

        scores.push_back({c.label, prob, l_ent, l_not});
    }

    std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) {
        return a.probability > b.probability;
    });
    return scores;
}

RoutingDecision ModernBERTRouter::route(const std::string& user_query) {
    if (user_query.empty() || user_query.size() < 10) {
        return {"reasoning", "general", "low", "none"};
    }

    auto scores = rank_intents(user_query);
    if (scores.empty()) {
        return {"reasoning", "general", "low", "none"};
    }

    const auto& top = scores.front();
    std::string intent = top.label;
    std::string domain = "general";
    if (intent == "coding") domain = "software";
    else if (intent == "image" || intent == "audio") domain = "creative";

    std::string complexity = (user_query.size() > 300 || user_query.find("```") != std::string::npos) ? "high" : "low";
    std::string required_caps = (intent == "coding") ? "code_execution" : "none";

    std::cout << "[ModernBERTRouter] Routed intent: '" << intent << "' (prob: " 
              << top.probability << ") domain: " << domain << std::endl;
    return {intent, domain, complexity, required_caps};
}
