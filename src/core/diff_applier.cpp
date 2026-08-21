#include "llmcli/core/diff_applier.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

namespace llmcli::core {

static std::string trim_right(const std::string& str) {
    auto last = str.find_last_not_of(" \t\r\n");
    return (last == std::string::npos) ? "" : str.substr(0, last + 1);
}

static std::vector<std::string> split_lines(const std::string& str) {
    std::vector<std::string> lines;
    std::stringstream ss(str);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::vector<SearchReplaceBlock> extract_search_replace_blocks(
    const std::string& text,
    const std::string& default_filepath
) {
    std::vector<SearchReplaceBlock> blocks;

    // Pattern for SEARCH / REPLACE blocks
    // <<<<<<< SEARCH\n(search)\n=======\n(replace)\n>>>>>>>
    std::regex block_regex(
        R"((?:(?:File|Arquivo|Path):\s*`?([^\n`\r]+)`?\n)?<<<<<<<\s*SEARCH\r?\n([\s\S]*?)=======\r?\n([\s\S]*?)>>>>>>>)",
        std::regex::ECMAScript
    );

    std::regex file_header_regex(
        R"((?:^|\n)(?:[#*`\s]*)(?:File|Arquivo|Path):\s*`?([^\n`\r]+)`?)",
        std::regex::ECMAScript
    );

    auto words_begin = std::sregex_iterator(text.begin(), text.end(), block_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        std::string header_path = match[1].str();
        std::string search_part = match[2].str();
        std::string replace_part = match[3].str();

        std::string target_file = header_path;
        if (target_file.empty()) {
            target_file = default_filepath;
        }

        if (target_file.empty()) {
            // Inspect preceding text up to 250 characters before match
            size_t match_pos = static_cast<size_t>(match.position());
            size_t start_look = match_pos > 250 ? match_pos - 250 : 0;
            std::string preceding = text.substr(start_look, match_pos - start_look);

            auto hdr_begin = std::sregex_iterator(preceding.begin(), preceding.end(), file_header_regex);
            auto hdr_end = std::sregex_iterator();
            std::string last_hdr;
            for (auto h = hdr_begin; h != hdr_end; ++h) {
                last_hdr = (*h)[1].str();
            }
            if (!last_hdr.empty()) {
                target_file = last_hdr;
            }
        }

        // Clean target file path
        if (!target_file.empty()) {
            while (!target_file.empty() && (target_file.front() == '`' || target_file.front() == '*' || target_file.front() == ' ' || target_file.front() == '#')) {
                target_file.erase(0, 1);
            }
            while (!target_file.empty() && (target_file.back() == '`' || target_file.back() == '*' || target_file.back() == ' ' || target_file.back() == '\r')) {
                target_file.pop_back();
            }

            blocks.push_back({target_file, search_part, replace_part});
        }
    }

    return blocks;
}

std::vector<ToolCall> extract_json_tool_calls(const std::string& text) {
    std::vector<ToolCall> calls;

    // Search in ```json blocks
    std::regex code_block_regex(R"(```(?:json)?\s*([\s\S]*?)\s*```)", std::regex::icase);
    auto b_begin = std::sregex_iterator(text.begin(), text.end(), code_block_regex);
    auto b_end = std::sregex_iterator();

    for (auto it = b_begin; it != b_end; ++it) {
        std::string content = (*it)[1].str();
        auto lines = split_lines(content);

        // Check line by line JSON
        for (const auto& line : lines) {
            std::string trimmed = line;
            auto f = trimmed.find_first_not_of(" \t");
            auto l = trimmed.find_last_not_of(" \t");
            if (f != std::string::npos && l != std::string::npos) {
                trimmed = trimmed.substr(f, l - f + 1);
            }
            if (trimmed.size() >= 2 && trimmed.front() == '{' && trimmed.back() == '}') {
                try {
                    json j = json::parse(trimmed);
                    std::string name = j.value("name", j.value("tool", j.value("action", "")));
                    json args = j.contains("arguments") ? j["arguments"] : (j.contains("args") ? j["args"] : (j.contains("parameters") ? j["parameters"] : json::object()));
                    if (!name.empty()) {
                        calls.push_back({ "call_" + std::to_string(calls.size()), name, args });
                    }
                } catch (...) {}
            }
        }

        // Check entire block as single JSON
        if (calls.empty()) {
            try {
                json j = json::parse(content);
                std::string name = j.value("name", j.value("tool", j.value("action", "")));
                json args = j.contains("arguments") ? j["arguments"] : (j.contains("args") ? j["args"] : (j.contains("parameters") ? j["parameters"] : json::object()));
                if (!name.empty()) {
                    calls.push_back({ "call_" + std::to_string(calls.size()), name, args });
                }
            } catch (...) {}
        }
    }

    return calls;
}

std::pair<bool, std::string> fuzzy_find_and_replace(
    const std::string& original_text,
    const std::string& search_text,
    const std::string& replace_text
) {
    // 1. Exact match
    size_t exact_pos = original_text.find(search_text);
    if (exact_pos != std::string::npos) {
        std::string res = original_text;
        res.replace(exact_pos, search_text.length(), replace_text);
        return {true, res};
    }

    // 2. Line-by-line whitespace-trimmed match
    auto orig_lines = split_lines(original_text);
    auto search_lines = split_lines(search_text);

    if (search_lines.empty() || orig_lines.size() < search_lines.size()) {
        return {false, original_text};
    }

    size_t search_len = search_lines.size();
    for (size_t i = 0; i <= orig_lines.size() - search_len; ++i) {
        bool match = true;
        for (size_t j = 0; j < search_len; ++j) {
            if (trim_right(orig_lines[i + j]) != trim_right(search_lines[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            std::ostringstream out;
            for (size_t b = 0; b < i; ++b) {
                out << orig_lines[b] << "\n";
            }
            out << replace_text;
            if (!replace_text.empty() && replace_text.back() != '\n') {
                out << "\n";
            }
            for (size_t a = i + search_len; a < orig_lines.size(); ++a) {
                out << orig_lines[a] << "\n";
            }
            return {true, out.str()};
        }
    }

    return {false, original_text};
}

std::string generate_unified_diff(
    const std::string& orig_text,
    const std::string& new_text,
    const std::string& filename
) {
    auto old_lines = split_lines(orig_text);
    auto new_lines = split_lines(new_text);

    std::ostringstream diff;
    diff << "--- a/" << filename << "\n";
    diff << "+++ b/" << filename << "\n";

    // Simple diff generator
    size_t i = 0, j = 0;
    while (i < old_lines.size() || j < new_lines.size()) {
        if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j]) {
            diff << " " << old_lines[i] << "\n";
            i++;
            j++;
        } else {
            if (i < old_lines.size()) {
                diff << "-" << old_lines[i] << "\n";
                i++;
            }
            if (j < new_lines.size()) {
                diff << "+" << new_lines[j] << "\n";
                j++;
            }
        }
    }

    return diff.str();
}

std::tuple<bool, std::string, std::string> apply_search_replace_block(
    const SearchReplaceBlock& block
) {
    Config& cfg = get_config();
    std::filesystem::path target_path = (cfg.project_root / block.file_path);

    try {
        target_path = std::filesystem::weakly_canonical(target_path);
    } catch (...) {}

    if (!cfg.is_path_safe(target_path)) {
        return {false, "Caminho inseguro fora do workspace: " + block.file_path, ""};
    }

    if (!std::filesystem::exists(target_path)) {
        // If search is empty or whitespace, create new file
        std::string s_clean = block.search_content;
        s_clean.erase(std::remove_if(s_clean.begin(), s_clean.end(), ::isspace), s_clean.end());

        if (s_clean.empty()) {
            if (target_path.has_parent_path()) {
                std::filesystem::create_directories(target_path.parent_path());
            }
            std::ofstream f(target_path);
            f << block.replace_content;
            std::string diff = "+ " + block.replace_content;
            return {true, "Arquivo criado: " + block.file_path, diff};
        }
        return {false, "Arquivo não encontrado para modificação: " + block.file_path, ""};
    }

    std::ifstream in(target_path);
    if (!in.is_open()) {
        return {false, "Falha ao abrir arquivo para leitura: " + block.file_path, ""};
    }
    std::stringstream buf;
    buf << in.rdbuf();
    std::string original_content = buf.str();
    in.close();

    auto [success, updated_content] = fuzzy_find_and_replace(original_content, block.search_content, block.replace_content);
    if (!success) {
        return {false, "Não foi possível localizar o trecho SEARCH no arquivo '" + block.file_path + "'.", ""};
    }

    std::string diff = generate_unified_diff(original_content, updated_content, block.file_path);

    std::ofstream out(target_path);
    if (!out.is_open()) {
        return {false, "Falha ao salvar arquivo modificado: " + block.file_path, ""};
    }
    out << updated_content;
    out.close();

    return {true, "Modificação aplicada com sucesso em '" + block.file_path + "'.", diff};
}

} // namespace llmcli::core
