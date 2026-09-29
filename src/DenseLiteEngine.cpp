#include "DenseLiteEngine.hpp"
#include "RequestAnalyzer.hpp"
#include "Formatter.hpp"
#include "Router.hpp"
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
#include "path_service.hpp"
#include "model_registry_db.hpp"
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
    session.working_memory = std::make_shared<WorkingMemory>();
    active_sessions[session.session_id] = session;
    return session;
}

void DenseLiteEngine::process(const std::string& request_body, httplib::Response& res) {
    OpenAIRequest parsed_req = RequestAnalyzer::parse_request(request_body);
    if (parsed_req.messages.empty()) {
        res.status = 400;
        res.set_content("{\"error\":{\"message\":\"Invalid request: 'messages' is a required non-empty array\",\"type\":\"invalid_request_error\"}}", "application/json");
        return;
    }
    
    InferenceSession session = get_or_create_session(parsed_req.session_id);
    parsed_req.session_id = session.session_id;

    std::cout << "[Engine] Processing Session: " << session.session_id << " | Iteration: " << session.iteration_count << std::endl;
    
    execute_pipeline(session, parsed_req, res);

    // Save state back or cleanup
    std::lock_guard<std::mutex> lock(engine_mutex);
    if (session.status == SessionStatus::COMPLETED || session.status == SessionStatus::FAILED) {
        active_sessions.erase(session.session_id);
        SessionToolRegistry::instance().clear_session(session.session_id);
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
    if (inf_cfg.routing_mode == "off" || (!parsed_req.model.empty() && parsed_req.model != "denselite")) {
        session.task_type = "general";
        std::cout << "[Engine] Routing bypassed (mode: " << inf_cfg.routing_mode << ", requested: " << parsed_req.model << ")" << std::endl;
    } else {
        RoutingDecision decision = Router::analyze_request(parsed_req);
        session.task_type = decision.intent;
    }
    if (session.working_memory) {
        session.working_memory->set_objective(session.task_type);
        session.working_memory->set_current_task(user_query);
    }

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
    if (inf_cfg.enable_tool_dedup && !requires_tools && (session.task_type == "text" || user_query.size() < 160)) {
        parsed_req.tools.clear();
        std::string default_sys = inf_cfg.system_prompt.empty() 
            ? "You are DenseLite, a fast, concise programming and chat assistant." 
            : inf_cfg.system_prompt;
        for (auto& msg : parsed_req.messages) {
            if (msg.role == "system" && msg.content.size() > 400) {
                msg.content = default_sys;
            }
        }
    } else if (requires_tools && parsed_req.tools.empty() && SessionToolRegistry::instance().has_tools(session.session_id)) {
        // Restore cached session tools if needed
        parsed_req.tools = SessionToolRegistry::instance().get_all_tools(session.session_id);
    }

    // 3. TARGET MODEL SELECTION & CLOUD OFFLOADING
    std::string target_model = parsed_req.model;
    if (target_model == "denselite" || target_model.empty()) {
        std::string db_path = PathService::instance().settings_db();
        if (session.task_type == "coding") {
            target_model = ModelRegistryDB::get_model_for_role(db_path, "coder");
            if (target_model.empty()) target_model = "deepseek-r1-distill-qwen-1_5b-q4_k_m";
        } else {
            target_model = ModelRegistryDB::get_model_for_role(db_path, "general");
            if (target_model.empty()) target_model = "llama-3_2-1b-instruct-abliterated_i1-q4_k_m";
        }
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
        int top_k = SettingsManager::instance().get_memory_config().search_top_k;
        if (top_k <= 0) top_k = 5;
        search_hits = search_engine_.search(user_query, session.task_type, top_k);
    } else {
        std::cout << "[Engine] Dynamic Context Injection: DISABLED (client-only mode)" << std::endl;
    }
    size_t default_ctx = (inf_cfg.context_window > 0) ? static_cast<size_t>(inf_cfg.context_window) : 8192;
    size_t ctx_cap = (resource_governor_.assess_eviction_stage() >= EvictionStage::SHRINK_CONTEXT) 
        ? std::min<size_t>(4096, default_ctx) : default_ctx;
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
    } else if (session.task_type == "audio") {
        std::vector<uint8_t> pcm_data(user_query.begin(), user_query.end());
        auto audio_res = multimodal_engine_.transcribe_pcm_bytes(pcm_data);
        if (audio_res.success) {
            session.status = SessionStatus::COMPLETED;
            res.status = 200;
            res.set_content("{\"text\":\"" + Formatter::json_escape(audio_res.text) + "\",\"language\":\"" + audio_res.detected_language + "\"}", "application/json");
            return;
        }
    }

    // 3. INFERENCE & INTERNAL CONTINUATION LOOP (Phase 6)
    session.status = SessionStatus::INFERRING;
    ModelEngine model_engine(models, router, &engine_mutex);
    const int MAX_INTERNAL_TURNS = 3;

    for (int turn = 0; turn < MAX_INTERNAL_TURNS; ++turn) {
        std::string output;
        int status_code = 0;
        bool step_ok = false;

        for (int retry = 0; retry < 3; ++retry) {
            status_code = model_engine.infer(target_model, parsed_req, prompt, output);
            if (status_code == 200) { step_ok = true; break; }

            RecoveryAction action = RecoveryPolicy::determine_action(status_code, output);
            auto resolve_fallback = [&]() -> std::string {
                std::string s = ModelRegistryDB::get_model_for_role(PathService::instance().settings_db(), "general");
                if (!s.empty()) return s;
                if (models.count("general")) return "general";
                return "llama-3_2-1b-instruct-abliterated_i1-q4_k_m";
            };

            if (action == RecoveryAction::REDUCE_CONTEXT) {
                opt_result = context_engine_.optimize_and_compile(parsed_req, {}, target_model, 4096);
                prompt = opt_result.compiled_prompt;
            } else if (action == RecoveryAction::SWITCH_MODEL || action == RecoveryAction::SWITCH_PROVIDER || action == RecoveryAction::SWITCH_KEY) {
                target_model = router.get_cheapest_model_for_provider("OPENROUTER", "text");
                if (target_model.empty()) target_model = resolve_fallback();
            } else if (action == RecoveryAction::FALLBACK_LOCAL) {
                target_model = resolve_fallback();
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
            std::string s = ModelRegistryDB::get_model_for_role(PathService::instance().settings_db(), "general");
            target_model = s.empty() ? "llama-3_2-1b-instruct-abliterated_i1-q4_k_m" : s;
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
    if (parsed_req.stream) {
        std::string sse_response = Formatter::format_sse_delta(final_output) + Formatter::format_sse_done();
        res.set_content(sse_response, "text/event-stream");
    } else {
        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string escaped_out = Formatter::json_escape(final_output);
        std::string json_res = "{\"id\":\"chatcmpl-" + session.session_id + "\","
                               "\"object\":\"chat.completion\","
                               "\"created\":" + std::to_string(now_sec) + ","
                               "\"model\":\"" + target_model + "\","
                               "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"" + escaped_out + "\"},\"finish_reason\":\"stop\"}]}";
        res.set_content(json_res, "application/json");
    }
}
