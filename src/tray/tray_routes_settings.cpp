#include "tray_routes.hpp"
#include "settings_manager.hpp"
#include "database_migrator.hpp"
#include "../dependencies/json.hpp"
#include <iostream>

using json = nlohmann::json;

static json get_all_settings_json() {
    auto& sm = SettingsManager::instance();
    json j;
    auto srv = sm.get_server_config();
    j["server"] = {
        {"host", srv.host}, {"port", srv.port}, {"threads", srv.threads},
        {"max_payload_mb", srv.max_payload_mb}, {"enable_api_auth", srv.enable_api_auth},
        {"api_secret_key", srv.api_secret_key}, {"cors_allowed_origins", srv.cors_allowed_origins},
        {"n_batch", srv.n_batch}
    };
    auto rc = sm.get_resource_config();
    int ram_pct = (rc.ram_budget_percent <= 1.0f) ? static_cast<int>(rc.ram_budget_percent * 100.0f) : static_cast<int>(rc.ram_budget_percent);
    j["resource"] = {{"ram_budget_percent", ram_pct}, {"enable_gpu", rc.enable_gpu}, {"vram_budget_mb", rc.vram_budget_mb}};
    auto inf = sm.get_inference_config();
    j["inference"] = {
        {"needle3_mode", inf.needle3_mode}, {"enable_tool_dedup", inf.enable_tool_dedup},
        {"enable_context_injection", inf.enable_context_injection}, {"context_window", inf.context_window},
        {"default_temperature", inf.default_temperature}, {"default_top_p", inf.default_top_p},
        {"repeat_penalty", inf.repeat_penalty}, {"repeat_last_n", inf.repeat_last_n},
        {"top_k", inf.top_k}, {"min_p", inf.min_p},
        {"max_output_tokens", inf.max_output_tokens}, {"system_prompt", inf.system_prompt}
    };
    auto mm = sm.get_multimodal_config();
    j["multimodal"] = {
        {"sd_steps", mm.sd_steps}, {"sd_cfg_scale", mm.sd_cfg_scale},
        {"sd_width", mm.sd_width}, {"sd_height", mm.sd_height},
        {"sd_negative_prompt", mm.sd_negative_prompt},
        {"whisper_language", mm.whisper_language}, {"whisper_beam_size", mm.whisper_beam_size}
    };
    auto mem = sm.get_memory_config();
    j["memory"] = json{
        {"search_top_k", mem.search_top_k}, {"similarity_threshold", mem.similarity_threshold},
        {"max_snippet_lines", mem.max_snippet_lines}, {"excluded_paths", mem.excluded_paths}
    };
    auto cc = sm.get_cloud_config();
    j["cloud"] = json{
        {"openrouter_api_key", cc.openrouter_api_key}, {"gemini_api_key", cc.gemini_api_key},
        {"openai_api_key", cc.openai_api_key}, {"cloud_priority", cc.cloud_priority},
        {"cloud_fallback_enabled", cc.cloud_fallback_enabled}
    };
    auto st = sm.get_storage_config();
    j["storage"] = {{"models_dir", st.models_dir}, {"data_dir", st.data_dir}, {"kv_cache_dir", st.kv_cache_dir}, {"logs_dir", st.logs_dir}};
    auto log = sm.get_logging_config();
    j["logging"] = {{"level", log.level}, {"enable_file_logging", log.enable_file_logging}, {"enable_console", log.enable_console}, {"log_path", log.log_path}};
    return j;
}

static void apply_settings_json(const json& body) {
    auto& sm = SettingsManager::instance();
    if (body.contains("inference")) {
        auto inf = sm.get_inference_config(); auto j = body["inference"];
        if (j.contains("needle3_mode")) inf.needle3_mode = j["needle3_mode"];
        if (j.contains("enable_tool_dedup")) inf.enable_tool_dedup = j["enable_tool_dedup"];
        if (j.contains("enable_context_injection")) inf.enable_context_injection = j["enable_context_injection"];
        if (j.contains("context_window")) inf.context_window = j["context_window"];
        if (j.contains("default_temperature")) inf.default_temperature = j["default_temperature"];
        if (j.contains("default_top_p")) inf.default_top_p = j["default_top_p"];
        if (j.contains("repeat_penalty")) inf.repeat_penalty = j["repeat_penalty"];
        if (j.contains("repeat_last_n")) inf.repeat_last_n = j["repeat_last_n"];
        if (j.contains("top_k")) inf.top_k = j["top_k"];
        if (j.contains("min_p")) inf.min_p = j["min_p"];
        if (j.contains("max_output_tokens")) inf.max_output_tokens = j["max_output_tokens"];
        if (j.contains("system_prompt")) inf.system_prompt = j["system_prompt"];
        sm.set_inference_config(inf);
    }
    if (body.contains("multimodal")) {
        auto mm = sm.get_multimodal_config(); auto j = body["multimodal"];
        if (j.contains("sd_steps")) mm.sd_steps = j["sd_steps"];
        if (j.contains("sd_cfg_scale")) mm.sd_cfg_scale = j["sd_cfg_scale"];
        if (j.contains("sd_width")) mm.sd_width = j["sd_width"];
        if (j.contains("sd_height")) mm.sd_height = j["sd_height"];
        if (j.contains("sd_negative_prompt")) mm.sd_negative_prompt = j["sd_negative_prompt"];
        if (j.contains("whisper_language")) mm.whisper_language = j["whisper_language"];
        if (j.contains("whisper_beam_size")) mm.whisper_beam_size = j["whisper_beam_size"];
        sm.set_multimodal_config(mm);
    }
    if (body.contains("memory")) {
        auto mem = sm.get_memory_config(); auto j = body["memory"];
        if (j.contains("search_top_k")) mem.search_top_k = j["search_top_k"];
        if (j.contains("similarity_threshold")) mem.similarity_threshold = j["similarity_threshold"];
        if (j.contains("max_snippet_lines")) mem.max_snippet_lines = j["max_snippet_lines"];
        if (j.contains("excluded_paths")) mem.excluded_paths = j["excluded_paths"];
        sm.set_memory_config(mem);
    }
    if (body.contains("cloud")) {
        auto cc = sm.get_cloud_config(); auto j = body["cloud"];
        if (j.contains("openrouter_api_key")) cc.openrouter_api_key = j["openrouter_api_key"];
        if (j.contains("gemini_api_key")) cc.gemini_api_key = j["gemini_api_key"];
        if (j.contains("openai_api_key")) cc.openai_api_key = j["openai_api_key"];
        if (j.contains("cloud_priority")) cc.cloud_priority = j["cloud_priority"];
        if (j.contains("cloud_fallback_enabled")) cc.cloud_fallback_enabled = j["cloud_fallback_enabled"];
        sm.set_cloud_config(cc);
    }
    if (body.contains("server")) {
        auto srv = sm.get_server_config(); auto j = body["server"];
        if (j.contains("host")) srv.host = j["host"];
        if (j.contains("port")) srv.port = j["port"];
        if (j.contains("threads")) srv.threads = j["threads"];
        if (j.contains("max_payload_mb")) srv.max_payload_mb = j["max_payload_mb"];
        if (j.contains("enable_api_auth")) srv.enable_api_auth = j["enable_api_auth"];
        if (j.contains("api_secret_key")) srv.api_secret_key = j["api_secret_key"];
        if (j.contains("cors_allowed_origins")) srv.cors_allowed_origins = j["cors_allowed_origins"];
        if (j.contains("n_batch")) srv.n_batch = j["n_batch"];
        sm.set_server_config(srv);
    }
    if (body.contains("resource")) {
        auto rc = sm.get_resource_config(); auto j = body["resource"];
        if (j.contains("ram_budget_percent")) {
            float p = j["ram_budget_percent"];
            rc.ram_budget_percent = (p > 1.0f) ? (p / 100.0f) : p;
        }
        if (j.contains("enable_gpu")) rc.enable_gpu = j["enable_gpu"];
        if (j.contains("vram_budget_mb")) rc.vram_budget_mb = j["vram_budget_mb"];
        sm.set_resource_config(rc);
    }
    if (body.contains("storage")) {
        auto st = sm.get_storage_config(); auto j = body["storage"];
        if (j.contains("models_dir")) st.models_dir = j["models_dir"];
        if (j.contains("data_dir")) st.data_dir = j["data_dir"];
        if (j.contains("kv_cache_dir")) st.kv_cache_dir = j["kv_cache_dir"];
        if (j.contains("logs_dir")) st.logs_dir = j["logs_dir"];
        sm.set_storage_config(st);
    }
    if (body.contains("logging")) {
        auto log = sm.get_logging_config(); auto j = body["logging"];
        if (j.contains("level")) log.level = j["level"];
        if (j.contains("enable_file_logging")) log.enable_file_logging = j["enable_file_logging"];
        if (j.contains("enable_console")) log.enable_console = j["enable_console"];
        if (j.contains("log_path")) log.log_path = j["log_path"];
        sm.set_logging_config(log);
    }
}

namespace TrayRoutes {

void register_settings_routes(httplib::Server& svr, const std::string& base_dir) {
    svr.Get("/api/settings", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(get_all_settings_json().dump(), "application/json");
    });

    svr.Post("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        try {
            apply_settings_json(json::parse(req.body));
            res.set_content(json({{"success", true}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(json({{"success", false}, {"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get("/api/settings/export", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=\"denselite-settings-backup.json\"");
        res.set_content(get_all_settings_json().dump(2), "application/json");
    });

    svr.Post("/api/settings/import", [](const httplib::Request& req, httplib::Response& res) {
        try {
            apply_settings_json(json::parse(req.body));
            res.set_content(json({{"success", true}, {"message", "Settings imported successfully"}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(json({{"success", false}, {"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/settings/reset", [base_dir](const auto&, auto& res) {
        bool ok = DatabaseMigrator::reset_settings_to_defaults(base_dir);
        if (ok) SettingsManager::instance().reload();
        res.set_content(json({{"success", ok}, {"message", ok ? "Settings reset to defaults" : "Failed to reset"}}).dump(), "application/json");
    });
}

} // namespace TrayRoutes
