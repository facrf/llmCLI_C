#pragma once

#include <string>
#include <map>
#include <filesystem>

namespace llmcli::utils {

class EnvLoader {
public:
    // Directory that contains the running executable. Empty if unavailable.
    static std::filesystem::path executable_dir();

    // Load a .env file into the process environment
    static bool load_dotenv(const std::filesystem::path& dotenv_path);

    // Auto-discover and load .env from project root, parent dirs, current working dir, and home
    static bool auto_discover_and_load(const std::filesystem::path& start_path);

    // Save or update an environment variable into .env file
    static bool save_env_var(const std::string& key, const std::string& value, const std::filesystem::path& dotenv_path);

    // Get an environment variable with fallback
    static std::string get_env(const std::string& key, const std::string& default_val = "");

    // Set an environment variable
    static void set_env(const std::string& key, const std::string& value);

    // Check if env var is truthy (1, true, yes)
    static bool get_bool_env(const std::string& key, bool default_val = false);
};

} // namespace llmcli::utils
