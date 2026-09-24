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
    : models(resident_models), router(router) {}

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
    session.status = SessionStatus::ANALYZING;
    
    // 1. ROUTING
    session.status = SessionStatus::ROUTING;
    DenseModel* needle = nullptr;
    if (models.find("needle") != models.end()) {
        needle = &models.at("needle");
    }
    
    RoutingDecision decision = NeedleRouter::analyze_request(parsed_req, needle);
    session.task_type = decision.intent;
    std::cout << "[Engine] Intent: " << session.task_type << "\n";

    // 2. CONTEXT MANAGEMENT
    DenseModel* nomic = nullptr;
    if (models.find("nomic") != models.end()) {
        nomic = &models.at("nomic");
    }
    ContextManager::optimize_context(parsed_req, 8192, true, nomic);

    // 3. MODEL SELECTION & INFERRING
    session.status = SessionStatus::INFERRING;
    std::string prompt = RequestAnalyzer::compile_prompt(parsed_req);
    
    ModelEngine model_engine(models, router);
    
    // Choose model based on intent (stubbed for now, pick best general model)
    std::string target_model = router.get_cheapest_model_for_provider("GROQ", "text");
    if (target_model.empty()) target_model = "qwen_main";

    std::string output;
    int status_code = 0;
    int max_retries = 3;
    bool success = false;

    for (int attempt = 0; attempt < max_retries; ++attempt) {
        status_code = model_engine.infer(target_model, parsed_req, prompt, output);

        if (status_code == 200) {
            success = true;
            break;
        }

        ProviderErrorAnalysis analysis = ProviderErrorAnalyzer::analyze("UNKNOWN", target_model, status_code, output);
        RecoveryAction action = RecoveryPolicy::determine_action(status_code, output);
        
        if (action == RecoveryAction::FAIL_SESSION) {
            break;
        } else if (action == RecoveryAction::SWITCH_MODEL || action == RecoveryAction::SWITCH_PROVIDER) {
            std::cout << "[Engine] Recovering: Switching model/provider." << std::endl;
            target_model = router.get_cheapest_model_for_provider("OPENROUTER", "text");
            if (target_model.empty()) target_model = "qwen_main";
        } else if (action == RecoveryAction::FALLBACK_LOCAL) {
            std::cout << "[Engine] Recovering: Fallback to local model." << std::endl;
            target_model = "qwen_coder";
        } else {
            std::cout << "[Engine] Recovering: Retrying same model." << std::endl;
        }
    }

    if (!success) {
        session.status = SessionStatus::FAILED;
        res.status = status_code;
        res.set_content(output, "text/plain");
        return;
    }

    ResponseAction r_action = ResponseAnalyzer::analyze(output);
    session.iteration_results.push_back(output);

    if (!CompletionPolicy::is_acceptable(session.task_type, output, session.iteration_results)) {
        session.status = SessionStatus::CONTINUING;
        std::cout << "[Engine] Response incomplete. Would trigger multi-turn loop." << std::endl;
    } else {
        session.status = SessionStatus::COMPLETED;
    }

    // 4. CURATION
    std::string sse_response;
    if (session.status == SessionStatus::COMPLETED) {
        Curator curator;
        std::string final_curated_output = curator.consolidate(session.iteration_results, session.task_type);

        // 5. FORMATTING (SSE chunk format)
        sse_response += Formatter::format_sse_delta(final_curated_output);
        sse_response += Formatter::format_sse_done();
    } else {
        // Just return the chunk and let the client know it's not done (we don't send DONE)
        sse_response += Formatter::format_sse_delta(output);
        // Note: We don't send [DONE] here because the session is CONTINUING.
        // However, standard Zed client expects [DONE] to close the stream.
        // We will send DONE for now so the HTTP request completes. In a real multi-turn,
        // we might hold the connection open or use internal loops.
        sse_response += Formatter::format_sse_done();
    }
    res.set_content(sse_response, "text/event-stream");
}
