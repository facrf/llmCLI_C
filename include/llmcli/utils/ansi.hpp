#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace llmcli::ansi {

// Color definitions
inline constexpr const char* RESET       = "\033[0m";
inline constexpr const char* BOLD        = "\033[1m";
inline constexpr const char* DIM         = "\033[2m";
inline constexpr const char* ITALIC      = "\033[3m";
inline constexpr const char* UNDERLINE   = "\033[4m";

inline constexpr const char* BLACK       = "\033[30m";
inline constexpr const char* RED         = "\033[31m";
inline constexpr const char* GREEN       = "\033[32m";
inline constexpr const char* YELLOW      = "\033[33m";
inline constexpr const char* BLUE        = "\033[34m";
inline constexpr const char* MAGENTA     = "\033[35m";
inline constexpr const char* CYAN        = "\033[36m";
inline constexpr const char* WHITE       = "\033[37m";

inline constexpr const char* BOLD_RED     = "\033[1;31m";
inline constexpr const char* BOLD_GREEN   = "\033[1;32m";
inline constexpr const char* BOLD_YELLOW  = "\033[1;33m";
inline constexpr const char* BOLD_BLUE    = "\033[1;34m";
inline constexpr const char* BOLD_MAGENTA = "\033[1;35m";
inline constexpr const char* BOLD_CYAN    = "\033[1;36m";
inline constexpr const char* BOLD_WHITE   = "\033[1;37m";

// Helpers
std::string colorize(const std::string& text, const char* color);
std::string panel(const std::string& content, const std::string& title = "", const char* border_color = CYAN, int min_width = 70);
std::string format_diff(const std::string& diff_text, const std::string& filename = "");
std::string highlight_code(const std::string& code, const std::string& lang = "");
std::string format_markdown(const std::string& markdown_text);

void print_panel(const std::string& content, const std::string& title = "", const char* border_color = CYAN);
void print_table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows, const std::string& title = "");

} // namespace llmcli::ansi
