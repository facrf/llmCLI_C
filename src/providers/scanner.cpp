#include "llmcli/providers/scanner.hpp"
#include "llmcli/utils/http.hpp"
#include <iostream>
#include <thread>
#include <future>

namespace llmcli::providers {

HostScanner::HostScanner(const std::string& host, double timeout_seconds)
    : timeout_seconds_(timeout_seconds) {
    std::string clean = host;
    if (clean.rfind("http://", 0) == 0) clean = clean.substr(7);
    if (clean.rfind("https://", 0) == 0) clean = clean.substr(8);
    auto slash_pos = clean.find('/');
    if (slash_pos != std::string::npos) clean = clean.substr(0, slash_pos);
    auto colon_pos = clean.find(':');
    if (colon_pos != std::string::npos) clean = clean.substr(0, colon_pos);

    host_ = clean.empty() ? "127.0.0.1" : clean;
}

std::optional<DiscoveredService> HostScanner::probe_ollama(int port) {
    std::string base_url = "http://" + host_ + ":" + std::to_string(port);
    auto res_v = utils::HttpClient::get(base_url + "/api/version", {}, timeout_seconds_);
    if (res_v.status_code == 200) {
        std::string ver = "desconhecida";
        try {
            json data = json::parse(res_v.body);
            ver = data.value("version", "desconhecida");
        } catch (...) {}

        std::vector<std::string> models;
        auto res_t = utils::HttpClient::get(base_url + "/api/tags", {}, timeout_seconds_);
        if (res_t.status_code == 200) {
            try {
                json data = json::parse(res_t.body);
                if (data.contains("models") && data["models"].is_array()) {
                    for (const auto& m : data["models"]) {
                        if (m.contains("name") && m["name"].is_string()) {
                            models.push_back(m["name"].get<std::string>());
                        }
                    }
                }
            } catch (...) {}
        }

        DiscoveredService s;
        s.service_name = "Ollama";
        s.base_url = base_url;
        s.provider_type = "ollama";
        s.status = "ONLINE";
        s.version = ver;
        s.models = models;
        s.details = "Ollama v" + ver + " (" + std::to_string(models.size()) + " modelos)";
        return s;
    }
    return std::nullopt;
}

std::optional<DiscoveredService> HostScanner::probe_llamacpp(int port, const std::string& name) {
    std::string base_url = "http://" + host_ + ":" + std::to_string(port);

    // 1. /props
    auto res_p = utils::HttpClient::get(base_url + "/props", {}, timeout_seconds_);
    if (res_p.status_code == 200) {
        std::vector<std::string> models = {"default"};
        auto res_m = utils::HttpClient::get(base_url + "/v1/models", {}, timeout_seconds_);
        if (res_m.status_code == 200) {
            try {
                json data = json::parse(res_m.body);
                if (data.contains("data") && data["data"].is_array()) {
                    models.clear();
                    for (const auto& item : data["data"]) {
                        if (item.contains("id") && item["id"].is_string()) {
                            models.push_back(item["id"].get<std::string>());
                        }
                    }
                }
            } catch (...) {}
        }
        DiscoveredService s;
        s.service_name = name;
        s.base_url = base_url;
        s.provider_type = "llamacpp";
        s.status = "ONLINE";
        s.models = models;
        s.details = "llama.cpp server ativo";
        return s;
    }

    // 2. /v1/models
    auto res_m = utils::HttpClient::get(base_url + "/v1/models", {}, timeout_seconds_);
    if (res_m.status_code == 200) {
        std::vector<std::string> models = {"default"};
        try {
            json data = json::parse(res_m.body);
            if (data.contains("data") && data["data"].is_array() && !data["data"].empty()) {
                models.clear();
                for (const auto& item : data["data"]) {
                    if (item.contains("id") && item["id"].is_string()) {
                        models.push_back(item["id"].get<std::string>());
                    }
                }
            }
        } catch (...) {}

        DiscoveredService s;
        s.service_name = name;
        s.base_url = base_url;
        s.provider_type = "llamacpp";
        s.status = "ONLINE";
        s.models = models;
        s.details = "llama.cpp / API compatível";
        return s;
    }

    return std::nullopt;
}

std::optional<DiscoveredService> HostScanner::probe_openai_compatible(int port, const std::string& name, const std::string& ptype) {
    std::string base_url = "http://" + host_ + ":" + std::to_string(port) + "/v1";
    auto res = utils::HttpClient::get(base_url + "/models", {}, timeout_seconds_);
    if (res.status_code == 200) {
        std::vector<std::string> models;
        try {
            json data = json::parse(res.body);
            if (data.contains("data") && data["data"].is_array()) {
                for (const auto& item : data["data"]) {
                    if (item.contains("id") && item["id"].is_string()) {
                        models.push_back(item["id"].get<std::string>());
                    }
                }
            }
        } catch (...) {}

        DiscoveredService s;
        s.service_name = name;
        s.base_url = base_url;
        s.provider_type = ptype;
        s.status = "ONLINE";
        s.models = models;
        s.details = name + " ativo (" + std::to_string(models.size()) + " modelos)";
        return s;
    }
    return std::nullopt;
}

std::vector<DiscoveredService> HostScanner::scan() {
    std::vector<DiscoveredService> discovered;

    auto f_ollama = std::async(std::launch::async, [&](){ return probe_ollama(11434); });
    auto f_llama1 = std::async(std::launch::async, [&](){ return probe_llamacpp(8080, "llama.cpp"); });
    auto f_llama2 = std::async(std::launch::async, [&](){ return probe_llamacpp(8081, "llama.cpp (Alt)"); });
    auto f_lms    = std::async(std::launch::async, [&](){ return probe_openai_compatible(1234, "LM Studio", "lmstudio"); });
    auto f_vllm   = std::async(std::launch::async, [&](){ return probe_openai_compatible(8000, "vLLM / LocalAI", "vllm"); });

    auto r1 = f_ollama.get(); if (r1.has_value()) discovered.push_back(r1.value());
    auto r2 = f_llama1.get(); if (r2.has_value()) discovered.push_back(r2.value());
    auto r3 = f_llama2.get(); if (r3.has_value()) discovered.push_back(r3.value());
    auto r4 = f_lms.get();    if (r4.has_value()) discovered.push_back(r4.value());
    auto r5 = f_vllm.get();   if (r5.has_value()) discovered.push_back(r5.value());

    return discovered;
}

} // namespace llmcli::providers
