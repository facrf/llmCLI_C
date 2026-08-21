#include "llmcli/config.hpp"
#include "llmcli/utils/env.hpp"
#include "llmcli/i18n.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace llmcli {

static std::filesystem::path get_default_prefs_path() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".llmcli_preferences.json";
    }
    return ".llmcli_preferences.json";
}

UserPreferences::UserPreferences(const std::filesystem::path& prefs_path) {
    if (prefs_path.empty()) {
        prefs_path_ = get_default_prefs_path();
    } else {
        prefs_path_ = prefs_path;
    }
    data_ = {
        {"global", json::object()},
        {"models", json::object()}
    };
    load();
}

void UserPreferences::load() {
    if (std::filesystem::exists(prefs_path_)) {
        try {
            std::ifstream f(prefs_path_);
            json j;
            f >> j;
            if (j.is_object()) {
                if (j.contains("global") && j["global"].is_object()) {
                    data_["global"] = j["global"];
                }
                if (j.contains("models") && j["models"].is_object()) {
                    data_["models"] = j["models"];
                }
            }
        } catch (...) {
            // Ignore corrupted preference files
        }
    }
}

void UserPreferences::save() {
    try {
        if (prefs_path_.has_parent_path()) {
            std::filesystem::create_directories(prefs_path_.parent_path());
        }
        std::ofstream f(prefs_path_);
        f << data_.dump(2);
    } catch (...) {
        // Ignore save errors
    }
}

void UserPreferences::reset() {
    data_ = {
        {"global", json::object()},
        {"models", json::object()}
    };
    try {
        if (std::filesystem::exists(prefs_path_)) {
            std::filesystem::remove(prefs_path_);
        }
    } catch (...) {}
    i18n::set_active_language("pt-BR");
}

json UserPreferences::get_global_pref(const std::string& key, const json& default_val) const {
    if (data_.contains("global") && data_["global"].contains(key)) {
        return data_["global"][key];
    }
    return default_val;
}

void UserPreferences::set_global_pref(const std::string& key, const json& value) {
    data_["global"][key] = value;
    save();
}

json UserPreferences::get_model_pref(const std::string& model, const std::string& key, const json& default_val) const {
    if (data_.contains("models") && data_["models"].contains(model) && data_["models"][model].contains(key)) {
        return data_["models"][model][key];
    }
    return get_global_pref(key, default_val);
}

void UserPreferences::set_model_pref(const std::string& model, const std::string& key, const json& value) {
    if (model.empty()) return;
    if (!data_["models"].contains(model)) {
        data_["models"][model] = json::object();
    }
    data_["models"][model][key] = value;
    save();
}

void UserPreferences::apply_model_preferences(const std::string& model, Config& config) {
    if (model.empty()) return;
    set_global_pref("last_active_model", model);

    json yolo_val = get_model_pref(model, "yolo_mode");
    if (!yolo_val.is_null() && yolo_val.is_boolean()) {
        config.yolo_mode = yolo_val.get<bool>();
    }

    json temp_val = get_model_pref(model, "temperature");
    if (!temp_val.is_null() && temp_val.is_number()) {
        config.temperature = temp_val.get<double>();
    }

    json arch_mode = get_global_pref("architect_mode");
    if (!arch_mode.is_null() && arch_mode.is_boolean()) {
        config.architect_mode = arch_mode.get<bool>();
    }

    json arch_model = get_global_pref("architect_model");
    if (!arch_model.is_null() && arch_model.is_string()) {
        config.architect_model = arch_model.get<std::string>();
    }

    json lang_val = get_global_pref("language");
    if (!lang_val.is_null() && lang_val.is_string()) {
        config.language = lang_val.get<std::string>();
        i18n::set_active_language(config.language);
    }
}

// Config implementation
Config& Config::instance() {
    static Config cfg = Config::load();
    return cfg;
}

Config Config::load(const std::filesystem::path& root_path) {
    Config cfg;

    if (!root_path.empty()) {
        cfg.project_root = std::filesystem::absolute(root_path);
    } else {
        cfg.project_root = std::filesystem::current_path();
    }

    // Load .env files (project root, parent workspace, cwd, and user config)
    utils::EnvLoader::auto_discover_and_load(cfg.project_root);

    // Environment variables override
    cfg.default_model = utils::EnvLoader::get_env("DEFAULT_MODEL", "gemini/gemini-2.5-flash");
    cfg.active_model = cfg.default_model;
    cfg.architect_model = utils::EnvLoader::get_env("ARCHITECT_MODEL", "gemini/gemini-2.5-pro");
    cfg.language = utils::EnvLoader::get_env("LLMCLI_LANG", "pt-BR");
    cfg.yolo_mode = utils::EnvLoader::get_bool_env("YOLO_MODE", false);

    cfg.local_endpoints.llamacpp = utils::EnvLoader::get_env("LLAMACPP_BASE_URL", "http://localhost:8080");
    cfg.local_endpoints.ollama = utils::EnvLoader::get_env("OLLAMA_BASE_URL", "http://localhost:11434");
    cfg.local_endpoints.lmstudio = utils::EnvLoader::get_env("LMSTUDIO_BASE_URL", "http://localhost:1234/v1");
    cfg.local_endpoints.vllm = utils::EnvLoader::get_env("VLLM_BASE_URL", "http://localhost:8000/v1");

    i18n::set_active_language(cfg.language);

    // Apply user preferences
    UserPreferences& prefs = get_preferences();
    json last_model = prefs.get_global_pref("last_active_model");
    if (!last_model.is_null() && last_model.is_string() && std::getenv("DEFAULT_MODEL") == nullptr) {
        cfg.active_model = last_model.get<std::string>();
    }
    json repomap_pref = prefs.get_global_pref("enable_repomap");
    if (!repomap_pref.is_null() && repomap_pref.is_boolean()) {
        cfg.enable_repomap = repomap_pref.get<bool>();
    }
    json repomap_max = prefs.get_global_pref("repomap_max_files");
    if (!repomap_max.is_null() && repomap_max.is_number()) {
        cfg.repomap_max_files = repomap_max.get<int>();
    }
    prefs.apply_model_preferences(cfg.active_model, cfg);

    return cfg;
}

bool Config::is_path_safe(const std::filesystem::path& target_path) const {
    if (!security.workspace_only) return true;
    try {
        std::filesystem::path abs_target = std::filesystem::weakly_canonical(target_path);
        std::filesystem::path abs_root = std::filesystem::weakly_canonical(project_root);

        auto it_target = abs_target.begin();
        auto it_root = abs_root.begin();

        while (it_root != abs_root.end()) {
            if (it_target == abs_target.end() || *it_target != *it_root) {
                return false;
            }
            ++it_target;
            ++it_root;
        }
        return true;
    } catch (...) {
        return false;
    }
}

static std::unique_ptr<UserPreferences> g_preferences_instance;
static std::unique_ptr<Config> g_config_instance;

UserPreferences& get_preferences() {
    if (!g_preferences_instance) {
        g_preferences_instance = std::make_unique<UserPreferences>();
    }
    return *g_preferences_instance;
}

Config& get_config() {
    if (!g_config_instance) {
        g_config_instance = std::make_unique<Config>(Config::load());
    }
    return *g_config_instance;
}

} // namespace llmcli
