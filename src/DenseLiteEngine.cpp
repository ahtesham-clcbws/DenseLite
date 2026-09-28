#include "DenseLiteEngine.hpp"
#include "RequestAnalyzer.hpp"
#include "Formatter.hpp"
#include "NeedleRouter.hpp"
#include "ContextManager.hpp"
#include "ModelEngine.hpp"
#include "Curator.hpp"
#include "ResponseAnalyzer.hpp"
#include "ProviderErrorAnalyzer.hpp"
#include "RecoveryPolicy.hpp"
#include "CompletionPolicy.hpp"
#include "SessionToolRegistry.hpp"
#include "SessionKVCache.hpp"
#include "database_paths.hpp"
#include "settings_manager.hpp"
#include <iostream>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <cctype>

static std::mutex engine_mutex;
static std::map<std::string, InferenceSession> active_sessions;

DenseLiteEngine::DenseLiteEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router, const std::string& base_dir)
    : models(resident_models), router(router), context_engine_(&tokenizer_registry_),
      search_engine_(&code_indexer_, &memory_engine_) {
    for (const auto& pair : models) {
        tokenizer_registry_.register_tokenizer(pair.first, &pair.second.vocab, pair.second.config.eos_token_id);
    }
    memory_engine_.init(DatabasePaths::memory_db(base_dir));
    code_indexer_.init(DatabasePaths::symbols_db(base_dir));
}

InferenceSession DenseLiteEngine::get_or_create_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    if (!session_id.empty() && active_sessions.find(session_id) != active_sessions.end()) {
        active_sessions[session_id].iteration_count++;
        active_sessions[session_id].status = SessionStatus::CONTINUING;
        return active_sessions[session_id];
    }
    
    InferenceSession session;
    if (session_id.empty()) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        session.session_id = std::to_string(ms);
    } else {
        session.session_id = session_id;
    }
    session.iteration_count = 1;
    session.status = SessionStatus::CREATED;
    active_sessions[session.session_id] = session;
    return session;
}

void DenseLiteEngine::process(const std::string& request_body, httplib::Response& res) {
    OpenAIRequest parsed_req = RequestAnalyzer::parse_request(request_body);
    
    InferenceSession session = get_or_create_session(parsed_req.session_id);
    parsed_req.session_id = session.session_id;

    std::cout << "[Engine] Processing Session: " << session.session_id << " | Iteration: " << session.iteration_count << std::endl;
    
    execute_pipeline(session, parsed_req, res);

    // Save state back or cleanup
    std::lock_guard<std::mutex> lock(engine_mutex);
    if (session.status == SessionStatus::COMPLETED || session.status == SessionStatus::FAILED) {
        active_sessions.erase(session.session_id);
    } else {
        active_sessions[session.session_id] = session;
    }
}

void DenseLiteEngine::execute_pipeline(InferenceSession& session, OpenAIRequest& parsed_req, httplib::Response& res) {
    ResourceGovernor::enforce_thread_limits();
    session.status = SessionStatus::ANALYZING;
    std::string user_query = parsed_req.messages.empty() ? "" : parsed_req.messages.back().content;

    auto inf_cfg = SettingsManager::instance().get_inference_config();

    // 1. ROUTING & RECALL
    session.status = SessionStatus::ROUTING;
    if (inf_cfg.needle3_mode == "off" || (!parsed_req.model.empty() && parsed_req.model != "denselite")) {
        session.task_type = "general";
        std::cout << "[Engine] Needle3 bypassed (mode: " << inf_cfg.needle3_mode << ", requested: " << parsed_req.model << ")" << std::endl;
    } else {
        DenseModel* needle = (models.find("needle") != models.end()) ? &models.at("needle") : nullptr;
        if (!needle && models.find("smollm2") != models.end()) {
            needle = &models.at("smollm2");
        }
        RoutingDecision decision = NeedleRouter::analyze_request(parsed_req, needle);
        session.task_type = decision.intent;
    }
    memory_engine_.working().set_objective(session.task_type);
    memory_engine_.working().set_current_task(user_query);

    // 2. CONTEXT SANITIZATION & TOOL PRUNING
    if (!parsed_req.tools.empty()) {
        SessionToolRegistry::instance().register_tools(session.session_id, parsed_req.tools);
    }

    bool requires_tools = false;
    std::string lower_query = user_query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    const std::vector<std::string> tool_keywords = {
        "artisan", "run", "execute", "migrate", "database", "query", "schema", 
        "tool", "search_symbols", "fetch", "bash", "command", "mcp"
    };
    for (const auto& kw : tool_keywords) {
        if (lower_query.find(kw) != std::string::npos) {
            requires_tools = true;
            break;
        }
    }

    // If query is conversational / simple text without explicit tool demand, strip tool bloat
    if (!requires_tools && (session.task_type == "text" || user_query.size() < 160)) {
        parsed_req.tools.clear();
        for (auto& msg : parsed_req.messages) {
            if (msg.role == "system" && msg.content.size() > 400) {
                msg.content = "You are DenseLite, a fast, concise programming and chat assistant.";
            }
        }
    } else if (requires_tools && parsed_req.tools.empty() && SessionToolRegistry::instance().has_tools(session.session_id)) {
        // Restore cached session tools if needed
        parsed_req.tools = SessionToolRegistry::instance().get_all_tools(session.session_id);
    }

    // 3. TARGET MODEL SELECTION & CLOUD OFFLOADING
    std::string target_model = parsed_req.model;
    if (target_model == "denselite" || target_model.empty()) {
        target_model = (session.task_type == "coding") ? "qwen_coder" : "qwen_main";
    }

    // Calculate total character footprint
    size_t total_payload_chars = 0;
    for (const auto& msg : parsed_req.messages) total_payload_chars += msg.content.size();

    // If payload is heavy (> 4000 chars) or requires tools, and requested model is auto (denselite),
    // offload to Cloud to prevent freezing the dual-core CPU
    if ((total_payload_chars > 4000 || requires_tools) && (parsed_req.model == "denselite" || parsed_req.model.empty())) {
        std::string cloud_model = router.get_provider_for_model("openai/gpt-oss-20b").empty() ? "" : "openai/gpt-oss-20b";
        if (cloud_model.empty()) cloud_model = router.get_provider_for_model("gemini-2.5-flash").empty() ? "" : "gemini-2.5-flash";
        if (!cloud_model.empty()) {
            std::cout << "[Engine] Heavy context / tool payload detected (" << total_payload_chars 
                      << " chars). Offloading to cloud: " << cloud_model << " to protect CPU." << std::endl;
            target_model = cloud_model;
        }
    }

    // Phase 7: Proactive Resource Throttling & Cloud Fallback
    if (resource_governor_.should_route_to_cloud()) {
        std::string cloud_fallback = router.get_cheapest_model_for_provider("OPENROUTER", "text");
        if (!cloud_fallback.empty()) target_model = cloud_fallback;
    }

    std::vector<SearchResult> search_hits;
    if (parsed_req.use_context && inf_cfg.enable_context_injection) {
        search_hits = search_engine_.search(user_query, session.task_type, 5);
    } else {
        std::cout << "[Engine] Dynamic Context Injection: DISABLED (client-only mode)" << std::endl;
    }
    size_t ctx_cap = (resource_governor_.assess_eviction_stage() >= EvictionStage::SHRINK_CONTEXT) ? 4096 : 8192;
    auto opt_result = context_engine_.optimize_and_compile(parsed_req, search_hits, target_model, ctx_cap);
    std::string prompt = opt_result.compiled_prompt;
    resource_governor_.track_inference_memory(prompt.size());

    // Phase 8: Multimodal Dispatch (Image / Audio)
    if (session.task_type == "image") {
        auto img_res = multimodal_engine_.generate_image(user_query);
        if (img_res.success) {
            session.status = SessionStatus::COMPLETED;
            res.status = 200;
            res.set_content("{\"created\":1700000000,\"data\":[{\"b64_json\":\"[image_data:" + std::to_string(img_res.data_bytes) + "_bytes]\"}]}", "application/json");
            return;
        }
    }

    // 3. INFERENCE & INTERNAL CONTINUATION LOOP (Phase 6)
    session.status = SessionStatus::INFERRING;
    ModelEngine model_engine(models, router);
    const int MAX_INTERNAL_TURNS = 3;

    for (int turn = 0; turn < MAX_INTERNAL_TURNS; ++turn) {
        std::string output;
        int status_code = 0;
        bool step_ok = false;

        for (int retry = 0; retry < 3; ++retry) {
            status_code = model_engine.infer(target_model, parsed_req, prompt, output);
            if (status_code == 200) { step_ok = true; break; }

            RecoveryAction action = RecoveryPolicy::determine_action(status_code, output);
            if (action == RecoveryAction::REDUCE_CONTEXT) {
                opt_result = context_engine_.optimize_and_compile(parsed_req, {}, target_model, 4096);
                prompt = opt_result.compiled_prompt;
            } else if (action == RecoveryAction::SWITCH_MODEL || action == RecoveryAction::SWITCH_PROVIDER || action == RecoveryAction::SWITCH_KEY) {
                target_model = router.get_cheapest_model_for_provider("OPENROUTER", "text");
                if (target_model.empty()) target_model = "qwen_main";
            } else if (action == RecoveryAction::FALLBACK_LOCAL) {
                target_model = "qwen_main";
            } else if (action == RecoveryAction::FAIL_SESSION) {
                break;
            }
        }

        if (!step_ok) {
            session.status = SessionStatus::FAILED;
            res.status = status_code;
            res.set_content(output, "text/plain");
            return;
        }

        session.iteration_results.push_back(output);
        ResponseAction action = ResponseAnalyzer::analyze(output);

        if (action == ResponseAction::TOOL_CALL) {
            session.status = SessionStatus::WAITING_FOR_TOOL;
            res.set_content(output, "application/json");
            return;
        }

        if (action == ResponseAction::INVALID) {
            target_model = "qwen_main"; // Fallback to safe local model
            continue;
        }

        if (action == ResponseAction::MODEL_CONTINUE) {
            session.status = SessionStatus::CONTINUING;
            prompt += "\n" + output + "\nContinue directly:";
            continue;
        }

        CompletionEvidence evidence;
        evidence.has_output = !output.empty();
        evidence.has_task_type = !session.task_type.empty();
        evidence.artifact_produced = (output.find("```") != std::string::npos);
        evidence.evidence_says_complete = (action == ResponseAction::COMPLETE);

        if (CompletionPolicy::is_acceptable(session.task_type, output, session.iteration_results, evidence)) {
            session.status = SessionStatus::COMPLETED;
            break;
        }
    }

    // 4. CURATION & FORMATTING
    Curator curator;
    std::string final_output = curator.consolidate(session.iteration_results, session.task_type, search_hits);
    std::string sse_response = Formatter::format_sse_delta(final_output) + Formatter::format_sse_done();
    res.set_content(sse_response, "text/event-stream");
}
