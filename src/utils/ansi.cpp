#include "llmcli/utils/ansi.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace llmcli::ansi {

std::string colorize(const std::string& text, const char* color) {
    return std::string(color) + text + RESET;
}

// Calculate visual length ignoring ANSI codes
static size_t visual_length(const std::string& str) {
    size_t len = 0;
    bool in_escape = false;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\033') {
            in_escape = true;
        } else if (in_escape) {
            if (str[i] == 'm') {
                in_escape = false;
            }
        } else {
            // Count UTF-8 lead bytes / ASCII characters
            if ((static_cast<unsigned char>(str[i]) & 0xC0) != 0x80) {
                len++;
            }
        }
    }
    return len;
}

std::string panel(const std::string& content, const std::string& title, const char* border_color, int min_width) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string line;
    size_t max_len = 0;

    while (std::getline(ss, line)) {
        lines.push_back(line);
        max_len = std::max(max_len, visual_length(line));
    }
    if (!title.empty()) {
        max_len = std::max(max_len, visual_length(title) + 4);
    }

    size_t width = std::max(static_cast<size_t>(min_width), max_len + 4);

    std::ostringstream out;
    // Top border
    out << border_color << "╭─";
    if (!title.empty()) {
        out << " " << BOLD << title << RESET << border_color << " ";
        size_t title_vis = visual_length(title) + 4;
        for (size_t i = title_vis; i < width; ++i) out << "─";
    } else {
        for (size_t i = 2; i < width; ++i) out << "─";
    }
    out << "╮" << RESET << "\n";

    // Content lines
    for (const auto& l : lines) {
        size_t vlen = visual_length(l);
        out << border_color << "│" << RESET << " " << l;
        for (size_t i = vlen + 1; i < width - 1; ++i) {
            out << " ";
        }
        out << border_color << "│" << RESET << "\n";
    }

    // Bottom border
    out << border_color << "╰";
    for (size_t i = 1; i < width - 1; ++i) out << "─";
    out << "╯" << RESET;

    return out.str();
}

void print_panel(const std::string& content, const std::string& title, const char* border_color) {
    std::cout << panel(content, title, border_color) << std::endl;
}

std::string format_diff(const std::string& diff_text, const std::string& filename) {
    if (diff_text.empty()) return "";

    std::ostringstream out;
    std::stringstream ss(diff_text);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.rfind("+++", 0) == 0 || line.rfind("---", 0) == 0) {
            out << BOLD_MAGENTA << line << RESET << "\n";
        } else if (line.rfind("@@", 0) == 0) {
            out << CYAN << line << RESET << "\n";
        } else if (line.rfind("+", 0) == 0) {
            out << GREEN << line << RESET << "\n";
        } else if (line.rfind("-", 0) == 0) {
            out << RED << line << RESET << "\n";
        } else {
            out << line << "\n";
        }
    }

    std::string title = filename.empty() ? "Diff Proposto" : "Modificações: " + filename;
    return panel(out.str(), title, YELLOW);
}

void print_table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows, const std::string& title) {
    if (headers.empty()) return;

    std::vector<size_t> col_widths(headers.size(), 0);
    for (size_t i = 0; i < headers.size(); ++i) {
        col_widths[i] = visual_length(headers[i]);
    }
    for (const auto& row : rows) {
        for (size_t i = 0; i < std::min(row.size(), col_widths.size()); ++i) {
            col_widths[i] = std::max(col_widths[i], visual_length(row[i]));
        }
    }

    // Add padding
    for (auto& w : col_widths) w += 2;

    if (!title.empty()) {
        std::cout << "\n" << BOLD_CYAN << "=== " << title << " ===" << RESET << "\n";
    }

    // Header line
    std::cout << BOLD;
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << headers[i];
        size_t pad = col_widths[i] > visual_length(headers[i]) ? col_widths[i] - visual_length(headers[i]) : 1;
        std::cout << std::string(pad, ' ');
    }
    std::cout << RESET << "\n";

    // Separator line
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::string(col_widths[i] - 1, '-') << " ";
    }
    std::cout << "\n";

    // Rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size(); ++i) {
            std::string cell = (i < row.size()) ? row[i] : "";
            std::cout << cell;
            size_t pad = col_widths[i] > visual_length(cell) ? col_widths[i] - visual_length(cell) : 1;
            std::cout << std::string(pad, ' ');
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

std::string highlight_code(const std::string& code, const std::string& /*lang*/) {
    std::vector<std::string> keywords = {
        "class", "struct", "enum", "union", "public", "private", "protected",
        "virtual", "override", "final", "template", "typename", "namespace",
        "using", "auto", "const", "constexpr", "static", "inline", "explicit",
        "if", "else", "switch", "case", "default", "while", "for", "do", "break",
        "continue", "return", "try", "catch", "throw", "new", "delete", "sizeof",
        "int", "void", "char", "bool", "double", "float", "size_t", "nullptr", "NULL",
        "true", "false", "include", "import", "from", "def", "async", "await",
        "function", "let", "var", "std"
    };

    std::ostringstream out;
    size_t i = 0;
    size_t n = code.size();

    while (i < n) {
        // Line comment // or #
        if ((i + 1 < n && code[i] == '/' && code[i + 1] == '/') || code[i] == '#') {
            size_t end_line = code.find('\n', i);
            if (end_line == std::string::npos) end_line = n;
            out << DIM << code.substr(i, end_line - i) << RESET;
            i = end_line;
            continue;
        }

        // Block comment /* ... */
        if (i + 1 < n && code[i] == '/' && code[i + 1] == '*') {
            size_t end_comment = code.find("*/", i + 2);
            if (end_comment == std::string::npos) end_comment = n - 2;
            out << DIM << code.substr(i, end_comment + 2 - i) << RESET;
            i = end_comment + 2;
            continue;
        }

        // String literals "..." or '...'
        if (code[i] == '"' || code[i] == '\'') {
            char quote = code[i];
            size_t j = i + 1;
            while (j < n && code[j] != quote) {
                if (code[j] == '\\' && j + 1 < n) j++;
                j++;
            }
            if (j < n) j++; // Include closing quote
            out << GREEN << code.substr(i, j - i) << RESET;
            i = j;
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(code[i])) && (i == 0 || !std::isalnum(static_cast<unsigned char>(code[i - 1])))) {
            size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(code[j])) || code[j] == '.' || code[j] == 'x' || code[j] == 'X')) {
                j++;
            }
            out << MAGENTA << code.substr(i, j - i) << RESET;
            i = j;
            continue;
        }

        // Identifiers / Keywords
        if (std::isalpha(static_cast<unsigned char>(code[i])) || code[i] == '_') {
            size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(code[j])) || code[j] == '_')) {
                j++;
            }
            std::string word = code.substr(i, j - i);
            bool is_kw = false;
            for (const auto& kw : keywords) {
                if (word == kw) {
                    is_kw = true;
                    break;
                }
            }
            if (is_kw) {
                out << BOLD_CYAN << word << RESET;
            } else {
                out << word;
            }
            i = j;
            continue;
        }

        // Punctuation and whitespace
        out << code[i];
        i++;
    }

    return out.str();
}

std::string format_markdown(const std::string& markdown_text) {
    std::ostringstream out;
    std::stringstream ss(markdown_text);
    std::string line;
    bool in_code_block = false;
    std::string code_lang;
    std::string code_buffer;

    while (std::getline(ss, line)) {
        if (line.rfind("```", 0) == 0) {
            if (!in_code_block) {
                in_code_block = true;
                code_lang = line.substr(3);
                auto f = code_lang.find_first_not_of(" \t\r");
                if (f != std::string::npos) code_lang = code_lang.substr(f);
                code_buffer.clear();
            } else {
                in_code_block = false;
                std::string header = code_lang.empty() ? "Código" : "Código (" + code_lang + ")";
                out << panel(highlight_code(code_buffer, code_lang), header, YELLOW) << "\n";
            }
            continue;
        }

        if (in_code_block) {
            code_buffer += line + "\n";
            continue;
        }

        // Headers
        if (line.rfind("### ", 0) == 0) {
            out << BOLD_YELLOW << "▸ " << line.substr(4) << RESET << "\n";
        } else if (line.rfind("## ", 0) == 0) {
            out << BOLD_CYAN << "■ " << line.substr(3) << RESET << "\n";
        } else if (line.rfind("# ", 0) == 0) {
            out << BOLD_MAGENTA << "█ " << line.substr(2) << RESET << "\n";
        } else if (line.rfind("> ", 0) == 0) {
            out << DIM << "│ " << line.substr(2) << RESET << "\n";
        } else {
            out << line << "\n";
        }
    }

    if (in_code_block && !code_buffer.empty()) {
        out << panel(highlight_code(code_buffer, code_lang), "Código", YELLOW) << "\n";
    }

    return out.str();
}

} // namespace llmcli::ansi
