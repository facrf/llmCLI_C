#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <utility>

namespace llmcli::providers {

class LLMProvider {
public:
    explicit LLMProvider(std::string model_name) : model_name_(std::move(model_name)) {}
    virtual ~LLMProvider() = default;

    const std::string& model_name() const { return model_name_; }

    // Streaming chat interface: calls on_chunk for each delta text and tool calls
    virtual bool chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::vector<ToolDefinition>& tools,
        std::function<void(const StreamChunk& chunk)> on_chunk,
        double temperature = 0.2,
        int max_tokens = 4096
    ) = 0;

    // Health / connectivity check
    virtual std::pair<bool, std::string> check_health() = 0;

    // List available models if supported
    virtual std::vector<std::string> list_available_models() {
        return {model_name_};
    }

protected:
    std::string model_name_;
};

} // namespace llmcli::providers
