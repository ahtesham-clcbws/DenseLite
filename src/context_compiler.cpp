#include "context_compiler.hpp"

std::string ContextCompiler::format_chatml(const std::vector<OpenAIMessage>& messages,
                                           bool append_assistant_header) {
    std::string prompt;
    for (const auto& msg : messages) {
        prompt += "<|im_start|>" + msg.role + "\n";
        prompt += msg.content + "\n<|im_end|>\n";
    }
    if (append_assistant_header) {
        prompt += "<|im_start|>assistant\n";
    }
    return prompt;
}

std::string ContextCompiler::format_llama3(const std::vector<OpenAIMessage>& messages,
                                           bool append_assistant_header) {
    std::string prompt = "<|begin_of_text|>";
    for (const auto& msg : messages) {
        prompt += "<|start_header_id|>" + msg.role + "<|end_header_id|>\n\n";
        prompt += msg.content + "<|eot_id|>";
    }
    if (append_assistant_header) {
        prompt += "<|start_header_id|>assistant<|end_header_id|>\n\n";
    }
    return prompt;
}

CompiledContext ContextCompiler::compile(const std::vector<OpenAIMessage>& messages,
                                         const Tokenizer* tokenizer,
                                         size_t max_input_tokens,
                                         bool append_assistant_header,
                                         const ModelConfig& config) {
    CompiledContext result;
    bool is_llama = false;
    bool is_chatml = false;

    if (!config.chat_template.empty()) {
        if (config.chat_template.find("<|start_header_id|>") != std::string::npos) {
            is_llama = true;
        } else if (config.chat_template.find("<|im_start|>") != std::string::npos) {
            is_chatml = true;
        }
    }
    
    // Fallback
    if (!is_llama && !is_chatml) {
        if (config.architecture.find("llama") != std::string::npos) {
            is_llama = true;
        } else {
            is_chatml = true; // Default
        }
    }

    if (is_llama) {
        result.prompt = format_llama3(messages, append_assistant_header);
    } else {
        result.prompt = format_chatml(messages, append_assistant_header);
    }

    auto count_fn = [&](const std::string& text) -> size_t {
        if (tokenizer && tokenizer->is_valid()) {
            return tokenizer->count_tokens(text);
        }
        return (text.size() + 3) / 4;
    };

    result.prompt_tokens = count_fn(result.prompt);

    // Calculate sectional breakdown
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        size_t msg_toks = count_fn(msg.content);
        if (msg.role == "system") {
            result.system_tokens += msg_toks;
        } else if (i + 1 == messages.size() && msg.role == "user") {
            result.task_tokens += msg_toks;
        } else {
            result.history_tokens += msg_toks;
        }
    }

    result.fits_budget = (result.prompt_tokens <= max_input_tokens);
    return result;
}
