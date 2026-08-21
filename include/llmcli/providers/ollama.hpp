#pragma once

#include "llmcli/providers/openai_compatible.hpp"

namespace llmcli::providers {

class OllamaProvider : public OpenAICompatibleProvider {
public:
    explicit OllamaProvider(
        const std::string& model_name = "qwen2.5-coder:latest",
        const std::string& base_url = "http://localhost:11434"
    );

    std::pair<bool, std::string> check_health() override;
    std::vector<std::string> list_available_models() override;

private:
    std::string root_url_;
};

} // namespace llmcli::providers
