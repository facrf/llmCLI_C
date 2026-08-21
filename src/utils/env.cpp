#include "llmcli/utils/env.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <vector>

namespace llmcli::utils {

static std::string trim(const std::string& str) {
    auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool EnvLoader::load_dotenv(const std::filesystem::path& dotenv_path) {
    if (!std::filesystem::exists(dotenv_path) || !std::filesystem::is_regular_file(dotenv_path)) {
        return false;
    }

    std::ifstream file(dotenv_path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Strip export if present
        if (line.rfind("export ", 0) == 0) {
            line = trim(line.substr(7));
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string val = trim(line.substr(eq_pos + 1));

        // Strip quotes
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        if (!key.empty()) {
            // Only set if not already present in environment
            if (std::getenv(key.c_str()) == nullptr) {
                set_env(key, val);
            }
        }
    }
    return true;
}

bool EnvLoader::auto_discover_and_load(const std::filesystem::path& start_path) {
    bool any_loaded = false;

    // 1. Direct path / project root
    if (load_dotenv(start_path / ".env")) {
        any_loaded = true;
    }

    // 2. Ascend parent directories up to git root or filesystem root
    try {
        std::filesystem::path cur = std::filesystem::absolute(start_path);
        for (int i = 0; i < 5 && cur.has_parent_path(); ++i) {
            std::filesystem::path parent = cur.parent_path();
            if (parent == cur) break;
            std::filesystem::path p_env = parent / ".env";
            if (std::filesystem::exists(p_env)) {
                if (load_dotenv(p_env)) {
                    any_loaded = true;
                }
            }
            if (std::filesystem::exists(parent / ".git")) {
                break;
            }
            cur = parent;
        }
    } catch (...) {}

    // 3. Current working directory
    try {
        std::filesystem::path cwd_env = std::filesystem::current_path() / ".env";
        if (std::filesystem::exists(cwd_env)) {
            if (load_dotenv(cwd_env)) {
                any_loaded = true;
            }
        }
    } catch (...) {}

    // 4. Home directory ~/.env and ~/.config/llmcli/.env
    const char* home = std::getenv("HOME");
    if (home) {
        std::filesystem::path home_path(home);
        if (load_dotenv(home_path / ".env")) {
            any_loaded = true;
        }
        if (load_dotenv(home_path / ".config" / "llmcli" / ".env")) {
            any_loaded = true;
        }
    }

    return any_loaded;
}

bool EnvLoader::save_env_var(const std::string& key, const std::string& value, const std::filesystem::path& dotenv_path) {
    if (key.empty()) return false;

    set_env(key, value);

    try {
        if (dotenv_path.has_parent_path() && !std::filesystem::exists(dotenv_path.parent_path())) {
            std::filesystem::create_directories(dotenv_path.parent_path());
        }

        std::vector<std::string> lines;
        bool key_updated = false;

        if (std::filesystem::exists(dotenv_path)) {
            std::ifstream in(dotenv_path);
            std::string line;
            while (std::getline(in, line)) {
                std::string trimmed = trim(line);
                if (trimmed.rfind("export ", 0) == 0) {
                    trimmed = trim(trimmed.substr(7));
                }
                auto eq_pos = trimmed.find('=');
                if (eq_pos != std::string::npos) {
                    std::string line_key = trim(trimmed.substr(0, eq_pos));
                    if (line_key == key) {
                        lines.push_back(key + "=" + value);
                        key_updated = true;
                        continue;
                    }
                }
                lines.push_back(line);
            }
        }

        if (!key_updated) {
            lines.push_back(key + "=" + value);
        }

        std::ofstream out(dotenv_path);
        if (!out.is_open()) return false;

        for (const auto& l : lines) {
            out << l << "\n";
        }
        out.flush();
        return true;
    } catch (...) {
        return false;
    }
}

std::string EnvLoader::get_env(const std::string& key, const std::string& default_val) {
    const char* val = std::getenv(key.c_str());
    if (val != nullptr) {
        return std::string(val);
    }
    return default_val;
}

void EnvLoader::set_env(const std::string& key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
}

bool EnvLoader::get_bool_env(const std::string& key, bool default_val) {
    std::string val = get_env(key, default_val ? "true" : "false");
    std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return std::tolower(c); });
    return (val == "1" || val == "true" || val == "yes" || val == "on");
}

} // namespace llmcli::utils
