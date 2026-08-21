#pragma once

#include "llmcli/providers/base.hpp"
#include <map>

namespace llmcli::providers {

class OpenAICompatibleProvider : public LLMProvider {
public:
    OpenAICompatibleProvider(
        const std::string& model_name,
        const std::string& base_url = "https://api.openai.com/v1",
        const std::string& api_key = "",
        const std::map<std::string, std::string>& extra_headers = {},
        double timeout_seconds = 120.0
    );

    bool chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::vector<ToolDefinition>& tools,
        std::function<void(const StreamChunk& chunk)> on_chunk,
        double temperature = 0.2,
        int max_tokens = 4096
    ) override;

    std::pair<bool, std::string> check_health() override;
    std::vector<std::string> list_available_models() override;

protected:
    std::string base_url_;
    std::string api_key_;
    std::map<std::string, std::string> headers_;
    double timeout_seconds_{120.0};

    std::map<std::string, std::string> build_headers() const;
    json convert_messages(const std::vector<ChatMessage>& messages) const;
    json convert_tools(const std::vector<ToolDefinition>& tools) const;
};

} // namespace llmcli::providers
