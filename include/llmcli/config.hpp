#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace llmcli {

using json = nlohmann::json;

struct LocalEndpoints {
    std::string llamacpp{"http://localhost:8080"};
    std::string ollama{"http://localhost:11434"};
    std::string lmstudio{"http://localhost:1234/v1"};
    std::string vllm{"http://localhost:8000/v1"};
};

struct GitConfig {
    bool auto_commit_on_edit{true};
    std::string commit_prefix{"llmCli:"};
};

struct SecurityConfig {
    bool workspace_only{true};
    int command_timeout_seconds{60};
};

class Config;

class UserPreferences {
public:
    explicit UserPreferences(const std::filesystem::path& prefs_path = "");

    void load();
    void save();
    void reset();

    json get_global_pref(const std::string& key, const json& default_val = nullptr) const;
    void set_global_pref(const std::string& key, const json& value);

    json get_model_pref(const std::string& model, const std::string& key, const json& default_val = nullptr) const;
    void set_model_pref(const std::string& model, const std::string& key, const json& value);

    void apply_model_preferences(const std::string& model, Config& config);

private:
    std::filesystem::path prefs_path_;
    json data_;
};

class Config {
public:
    std::filesystem::path project_root;
    std::string default_model{"gemini/gemini-2.5-flash"};
    std::string active_model;
    bool architect_mode{false};
    std::string architect_model{"gemini/gemini-2.5-pro"};
    std::string language{"pt-BR"};
    bool yolo_mode{false};
    bool dry_run{false};
    bool enable_repomap{true};
    int repomap_max_files{60};
    double temperature{0.2};
    int max_tokens{4096};

    LocalEndpoints local_endpoints;
    GitConfig git;
    SecurityConfig security;

    static Config& instance();
    static Config load(const std::filesystem::path& root_path = "");

    bool is_path_safe(const std::filesystem::path& target_path) const;
};

UserPreferences& get_preferences();
Config& get_config();

} // namespace llmcli
