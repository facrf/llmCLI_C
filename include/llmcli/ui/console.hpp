#pragma once

#include "llmcli/types.hpp"
#include "llmcli/providers/registry.hpp"
#include <string>
#include <vector>

namespace llmcli::ui {

void print_banner(const std::string& active_model, bool yolo_mode);
void print_diff(const std::string& diff_text, const std::string& filename = "");
void print_tool_execution(const std::string& tool_name, const std::string& args_summary, bool is_yolo = false);
void print_tool_result(const std::string& tool_name, bool success, const std::string& output);

std::string ask_user_confirmation(const std::string& prompt_text);

void print_status_table(const std::vector<providers::ProviderHealthItem>& status_list);
void print_scan_results(const std::string& host, const std::vector<DiscoveredService>& services);

} // namespace llmcli::ui
