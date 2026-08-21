#pragma once

#include <string>
#include <tuple>
#include <optional>
#include <vector>

namespace llmcli::tools {

std::tuple<int, std::string, std::string> run_git_cmd(const std::vector<std::string>& args);

bool is_git_repo();
std::string get_git_diff(bool cached = false);
std::string get_raw_git_diff();
std::string get_git_status();

std::optional<std::string> create_checkpoint_commit(const std::string& message);
std::pair<bool, std::string> undo_last_checkpoint();
std::pair<bool, std::string> create_user_commit(const std::string& message);

} // namespace llmcli::tools
