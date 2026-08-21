#include "llmcli/providers/llamacpp.hpp"
#include "llmcli/utils/http.hpp"

namespace llmcli::providers {

LlamaCppProvider::LlamaCppProvider(const std::string& model_name, const std::string& base_url)
    : OpenAICompatibleProvider(
        model_name,
        (base_url.rfind("/v1") != std::string::npos ? base_url : base_url + "/v1"),
        "sk-llamacpp"
    ) {}

std::pair<bool, std::string> LlamaCppProvider::check_health() {
    std::string root_url = base_url_;
    if (root_url.rfind("/v1") == root_url.size() - 3) {
        root_url = root_url.substr(0, root_url.size() - 3);
    }

    // 1. Try /health
    auto res_h = utils::HttpClient::get(root_url + "/health", {}, 4.0);
    if (res_h.status_code == 200) {
        return {true, "Servidor llama.cpp ativo (porta 8080)"};
    }

    // 2. Try /props
    auto res_p = utils::HttpClient::get(root_url + "/props", {}, 4.0);
    if (res_p.status_code == 200) {
        return {true, "Servidor llama.cpp ativo (/props OK)"};
    }

    // 3. Try /v1/models
    auto res_m = utils::HttpClient::get(base_url_ + "/models", {}, 4.0);
    if (res_m.status_code == 200) {
        return {true, "Servidor llama.cpp ativo (/v1/models OK)"};
    }

    return {false, "Servidor llama.cpp inacessível em " + root_url};
}

std::vector<std::string> LlamaCppProvider::list_available_models() {
    auto res = utils::HttpClient::get(base_url_ + "/models", {}, 4.0);
    if (res.success) {
        try {
            json data = json::parse(res.body);
            if (data.contains("data") && data["data"].is_array()) {
                std::vector<std::string> models;
                for (const auto& item : data["data"]) {
                    if (item.contains("id") && item["id"].is_string()) {
                        models.push_back(item["id"].get<std::string>());
                    }
                }
                if (!models.empty()) return models;
            }
        } catch (...) {}
    }
    return {model_name_};
}

} // namespace llmcli::providers
