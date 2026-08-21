#include "llmcli/providers/ollama.hpp"
#include "llmcli/utils/http.hpp"

namespace llmcli::providers {

OllamaProvider::OllamaProvider(const std::string& model_name, const std::string& base_url)
    : OpenAICompatibleProvider(
        model_name,
        (base_url.rfind("/v1") != std::string::npos ? base_url : base_url + "/v1"),
        "ollama"
    ), root_url_(base_url) {
    while (!root_url_.empty() && root_url_.back() == '/') {
        root_url_.pop_back();
    }
    if (root_url_.rfind("/v1") == root_url_.size() - 3) {
        root_url_ = root_url_.substr(0, root_url_.size() - 3);
    }
}

std::pair<bool, std::string> OllamaProvider::check_health() {
    auto res = utils::HttpClient::get(root_url_ + "/api/version", {}, 4.0);
    if (res.status_code == 200) {
        std::string ver = "ativo";
        try {
            json d = json::parse(res.body);
            if (d.contains("version") && d["version"].is_string()) {
                ver = "v" + d["version"].get<std::string>();
            }
        } catch (...) {}
        return {true, "Ollama ativo (" + ver + ")"};
    }
    return {false, "Ollama inacessível em " + root_url_};
}

std::vector<std::string> OllamaProvider::list_available_models() {
    auto res = utils::HttpClient::get(root_url_ + "/api/tags", {}, 4.0);
    if (res.success) {
        try {
            json data = json::parse(res.body);
            if (data.contains("models") && data["models"].is_array()) {
                std::vector<std::string> models;
                for (const auto& item : data["models"]) {
                    if (item.contains("name") && item["name"].is_string()) {
                        models.push_back(item["name"].get<std::string>());
                    }
                }
                if (!models.empty()) return models;
            }
        } catch (...) {}
    }
    return {model_name_};
}

} // namespace llmcli::providers
