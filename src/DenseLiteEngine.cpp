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
#include <iostream>
#include <chrono>
#include <mutex>

static std::mutex engine_mutex;
static std::map<std::string, InferenceSession> active_sessions;

DenseLiteEngine::DenseLiteEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router)
    : models(resident_models), router(router), context_engine_(&tokenizer_registry_),
      search_engine_(&code_indexer_, &memory_engine_) {
    for (const auto& pair : models) {
        tokenizer_registry_.register_tokenizer(pair.first, &pair.second.vocab, pair.second.config.eos_token_id);
    }
    memory_engine_.init("denselite_memory.db");
    code_indexer_.init("denselite_symbols.db");
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

    // 1. ROUTING & RECALL
    session.status = SessionStatus::ROUTING;
    DenseModel* needle = (models.find("needle") != models.end()) ? &models.at("needle") : nullptr;
    RoutingDecision decision = NeedleRouter::analyze_request(parsed_req, needle);
    session.task_type = decision.intent;
    memory_engine_.working().set_task(session.task_type, user_query);

    // 2. SEARCH & CONTEXT COMPILATION
    auto search_hits = search_engine_.search(user_query, session.task_type, 5);
    std::string target_model = parsed_req.model;
    if (target_model == "denselite" || target_model.empty()) {
        target_model = (session.task_type == "coding") ? "qwen_coder" : "qwen_main";
    }

    // Phase 7: Proactive Resource Throttling & Cloud Fallback
    if (resource_governor_.should_route_to_cloud()) {
        std::string cloud_fallback = router.get_cheapest_model_for_provider("OPENROUTER", "text");
        if (!cloud_fallback.empty()) target_model = cloud_fallback;
    }

    size_t ctx_cap = (resource_governor_.assess_eviction_stage() >= EvictionStage::SHRINK_CONTEXT) ? 4096 : 8192;
    auto opt_result = context_engine_.optimize_and_compile(parsed_req, search_hits, target_model, ctx_cap);
    std::string prompt = opt_result.compiled_prompt;
    resource_governor_.track_inference_memory(prompt.size());

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
