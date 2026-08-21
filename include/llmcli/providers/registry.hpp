#pragma once

#include "llmcli/providers/base.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace llmcli::providers {

struct ProviderHealthItem {
    std::string provider;
    std::string endpoint;
    std::string status; // "ONLINE", "CONFIGURADO", "SEM CHAVE", "OFFLINE"
    std::string detail;
    std::vector<std::string> models;
    std::string example;
};

class ProviderRegistry {
public:
    static std::shared_ptr<LLMProvider> create_provider(const std::string& model_string = "");

    static std::vector<ModelPreset> get_model_presets();
    static std::string resolve_model_by_id_or_name(const std::string& input_str);

    static std::vector<ProviderHealthItem> get_status_overview();
    static std::string find_backup_model(const std::string& current_model);
};

} // namespace llmcli::providers
