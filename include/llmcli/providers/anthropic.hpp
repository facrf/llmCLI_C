#pragma once

#include "llmcli/providers/base.hpp"

namespace llmcli::providers {

class AnthropicProvider : public LLMProvider {
public:
    explicit AnthropicProvider(
        const std::string& model_name = "claude-3-7-sonnet-20250219",
        const std::string& api_key = ""
    );

    bool chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::vector<ToolDefinition>& tools,
        std::function<void(const StreamChunk& chunk)> on_chunk,
        double temperature = 0.2,
        int max_tokens = 4096
    ) override;

    std::pair<bool, std::string> check_health() override;

private:
    std::string api_key_;

    std::pair<std::string, json> convert_messages(const std::vector<ChatMessage>& messages) const;
    json convert_tools(const std::vector<ToolDefinition>& tools) const;
};

} // namespace llmcli::providers
