#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <vector>
#include <tuple>
#include <optional>

namespace llmcli::core {

// Extract SEARCH/REPLACE blocks from LLM response
std::vector<SearchReplaceBlock> extract_search_replace_blocks(
    const std::string& text,
    const std::string& default_filepath = ""
);

// Extract JSON tool calls from markdown code blocks or lines
std::vector<ToolCall> extract_json_tool_calls(const std::string& text);

// Fuzzy find and replace algorithm
std::pair<bool, std::string> fuzzy_find_and_replace(
    const std::string& original_text,
    const std::string& search_text,
    const std::string& replace_text
);

// Apply a search/replace block to target file (returns {success, message, diff_str})
std::tuple<bool, std::string, std::string> apply_search_replace_block(
    const SearchReplaceBlock& block
);

// Generate unified diff between two strings
std::string generate_unified_diff(
    const std::string& orig_text,
    const std::string& new_text,
    const std::string& filename
);

} // namespace llmcli::core
