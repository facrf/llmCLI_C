#pragma once

#include <string>
#include <filesystem>

namespace llmcli::context {

std::string build_repo_map(const std::filesystem::path& project_root, int max_files = 80);

} // namespace llmcli::context
