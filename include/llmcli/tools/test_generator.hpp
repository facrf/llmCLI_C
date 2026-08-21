#pragma once

#include <string>
#include <filesystem>

namespace llmcli::tools {

std::string get_test_prompt_for_file(
    const std::filesystem::path& target_file,
    const std::filesystem::path& project_root = ""
);

} // namespace llmcli::tools
