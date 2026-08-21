#pragma once

#include "llmcli/providers/base.hpp"

namespace llmcli::providers {

class GeminiProvider : public LLMProvider {
public:
    explicit GeminiProvider(
        const std::string& model_name = "gemini-2.5-flash",
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

    std::pair<json, json> convert_contents(const std::vector<ChatMessage>& messages) const;
    json convert_tools(const std::vector<ToolDefinition>& tools) const;
};

} // namespace llmcli::providers
