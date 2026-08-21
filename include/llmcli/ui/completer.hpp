#pragma once

#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace llmcli::ui {

struct SlashCommandInfo {
    std::string command;
    std::string description;
};

const std::vector<SlashCommandInfo>& get_all_slash_commands();

// Resolves a slash command by prefix or alias
std::pair<std::optional<std::string>, std::vector<std::string>> resolve_slash_command(
    const std::string& command,
    bool has_arg = false
);

// File and path auto-suggestions
std::vector<std::string> complete_files(const std::string& prefix, const std::string& root_dir);

// Model suggestions
std::vector<std::string> complete_models(const std::string& prefix);

// Language suggestions
std::vector<std::string> complete_languages(const std::string& prefix);

// API key & env suggestions
std::vector<std::string> complete_keys(const std::string& prefix);

// Longest common prefix helper
std::string compute_longest_common_prefix(const std::vector<std::string>& candidates);

} // namespace llmcli::ui
