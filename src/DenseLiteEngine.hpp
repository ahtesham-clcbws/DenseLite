#pragma once
#include <string>
#include <map>
#include <vector>
#include "httplib.h"
#include "model.hpp"
#include "sqlite_router.hpp"
#include "Curator.hpp"
#include "RequestAnalyzer.hpp"

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

private:
    std::map<std::string, DenseModel>& models;
    SQLiteRouter& router;

    InferenceSession get_or_create_session(const std::string& session_id);
    void execute_pipeline(InferenceSession& session, OpenAIRequest& parsed_req, httplib::Response& res);
};
