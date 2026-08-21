#pragma once

#include "llmcli/providers/openai_compatible.hpp"

namespace llmcli::providers {

class LlamaCppProvider : public OpenAICompatibleProvider {
public:
    explicit LlamaCppProvider(
        const std::string& model_name = "default",
        const std::string& base_url = "http://localhost:8080"
    );

    std::pair<bool, std::string> check_health() override;
    std::vector<std::string> list_available_models() override;
};

} // namespace llmcli::providers
