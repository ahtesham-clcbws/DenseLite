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

CompiledContext ContextCompiler::compile(const std::vector<OpenAIMessage>& messages,
                                         const Tokenizer* tokenizer,
                                         size_t max_input_tokens,
                                         bool append_assistant_header) {
    CompiledContext result;
    result.prompt = format_chatml(messages, append_assistant_header);

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
