#include "llmcli/ui/repl.hpp"
#include "llmcli/config.hpp"
#include "llmcli/core/exporter.hpp"
#include "llmcli/i18n.hpp"
#include "llmcli/providers/registry.hpp"
#include "llmcli/providers/scanner.hpp"
#include "llmcli/tools/git_ops.hpp"
#include "llmcli/tools/test_generator.hpp"
#include "llmcli/ui/completer.hpp"
#include "llmcli/ui/console.hpp"
#include "llmcli/utils/ansi.hpp"
#include "llmcli/utils/env.hpp"
#include "llmcli/utils/http.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cctype>
#include <csignal>

namespace llmcli::ui {

namespace {

static struct termios g_clean_termios;
static bool g_clean_termios_saved = false;

struct RawTermModeGuard {
    struct termios orig_termios;
    bool enabled = false;

    RawTermModeGuard() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
            if (g_clean_termios_saved) orig_termios = g_clean_termios;
            else return;
        }

        if (!g_clean_termios_saved) {
            g_clean_termios = orig_termios;
            g_clean_termios_saved = true;
        }

        struct termios raw = g_clean_termios;
        raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != -1) {
            enabled = true;
        }
    }

    ~RawTermModeGuard() {
        if (enabled) {
            tcsetattr(STDIN_FILENO, TCSANOW, g_clean_termios_saved ? &g_clean_termios : &orig_termios);
        }
    }
};

static void print_completion_grid(const std::vector<std::string>& items, const std::string& title = "") {
    if (items.empty()) return;
    if (!title.empty()) {
        std::cout << "\n" << ansi::BOLD_CYAN << title << ansi::RESET << "\n";
    } else {
        std::cout << "\n";
    }

    size_t max_w = 0;
    for (const auto& it : items) {
        max_w = std::max(max_w, it.size());
    }
    max_w += 3; // padding

    int term_width = 80;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        term_width = ws.ws_col;
    }

    size_t cols = std::max(size_t(1), static_cast<size_t>(term_width) / max_w);
    size_t col_idx = 0;

    for (const auto& item : items) {
        std::cout << "  " << ansi::BOLD_YELLOW << item << ansi::RESET;
        size_t pad = (item.size() + 2 < max_w) ? max_w - (item.size() + 2) : 1;
        std::cout << std::string(pad, ' ');
        col_idx++;
        if (col_idx >= cols) {
            std::cout << "\n";
            col_idx = 0;
        }
    }
    if (col_idx != 0) std::cout << "\n";
}

} // namespace

ReplSession::ReplSession(std::shared_ptr<core::Agent> agent)
    : agent_(agent ? agent : std::make_shared<core::Agent>()) {
    const char* home = std::getenv("HOME");
    if (home) {
        history_path_ = std::filesystem::path(home) / ".llmcli_history";
    } else {
        history_path_ = ".llmcli_history";
    }
    load_history();
}

std::string ReplSession::get_prompt_str() const {
    Config& cfg = get_config();
    std::string model = cfg.active_model;
    auto last_slash = model.find_last_of('/');
    if (last_slash != std::string::npos) {
        model = model.substr(last_slash + 1);
    }

    std::string yolo_tag = cfg.yolo_mode ? std::string(ansi::BOLD_RED) + "⚡ YOLO: ON" + ansi::RESET
                                         : std::string(ansi::BOLD_GREEN) + "🛡️ YOLO: OFF" + ansi::RESET;

    std::ostringstream ss;
    ss << ansi::BOLD_CYAN << "llmCli" << ansi::RESET << " [" << ansi::YELLOW << model << ansi::RESET;

    if (cfg.architect_mode) {
        std::string arch_short = cfg.architect_model;
        auto as_pos = arch_short.find_last_of('/');
        if (as_pos != std::string::npos) arch_short = arch_short.substr(as_pos + 1);
        ss << " | " << ansi::BOLD_MAGENTA << "🏛️ ARCH: " << arch_short << ansi::RESET;
    }

    ss << " | " << yolo_tag;

    if (cfg.dry_run) {
        ss << " | " << ansi::BOLD_CYAN << "🔍 DRY-RUN: ON" << ansi::RESET;
    }

    size_t files_count = agent_->session().file_tracker().size();
    if (files_count > 0) {
        ss << " | files:" << files_count;
    }

    ss << "] " << ansi::BOLD_CYAN << "❯ " << ansi::RESET;
    return ss.str();
}

void ReplSession::load_history() {
    history_.clear();
    if (!std::filesystem::exists(history_path_)) return;
    try {
        std::ifstream f(history_path_);
        std::string line;
        while (std::getline(f, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!line.empty()) {
                history_.push_back(line);
            }
        }
    } catch (...) {}
}

void ReplSession::save_history_entry(const std::string& entry) {
    if (entry.empty()) return;
    if (!history_.empty() && history_.back() == entry) return;
    history_.push_back(entry);
    try {
        if (history_path_.has_parent_path() && !std::filesystem::exists(history_path_.parent_path())) {
            std::filesystem::create_directories(history_path_.parent_path());
        }
        std::ofstream f(history_path_, std::ios::app);
        if (f.is_open()) {
            f << entry << "\n";
            f.flush();
        }
    } catch (...) {}
}

std::string ReplSession::read_line_input(const std::string& prompt) {
    if (!isatty(STDIN_FILENO)) {
        std::cout << prompt << std::flush;
        std::string input;
        if (!std::getline(std::cin, input)) {
            return "/exit";
        }
        // Sanitize backspaces if any raw ^H or ^? was piped
        std::string clean;
        for (char ch : input) {
            if (ch == 127 || ch == 8) {
                if (!clean.empty()) clean.pop_back();
            } else {
                clean.push_back(ch);
            }
        }
        return clean;
    }

    RawTermModeGuard raw_guard;
    if (!raw_guard.enabled) {
        std::cout << prompt << std::flush;
        std::string input;
        if (!std::getline(std::cin, input)) {
            return "/exit";
        }
        return input;
    }

    std::string buffer;
    size_t cursor_pos = 0;
    int history_idx = static_cast<int>(history_.size());
    std::string saved_buf;
    int lines_rendered_below = 0;
    int popup_selected_idx = -1;

    auto clear_rendered_below = [&]() {
        if (lines_rendered_below > 0) {
            for (int i = 0; i < lines_rendered_below; ++i) {
                std::cout << "\n\033[2K";
            }
            std::cout << "\033[" << lines_rendered_below << "A";
            lines_rendered_below = 0;
        }
    };

    auto redraw = [&]() {
        clear_rendered_below();

        // 1. Draw prompt and buffer
        std::cout << "\r\033[2K" << prompt << buffer;

        // 2. If user is typing a slash command, render live popup suggestions below
        if (buffer.rfind("/", 0) == 0 && buffer.find(' ') == std::string::npos) {
            std::vector<SlashCommandInfo> live_matches;
            for (const auto& cmd_info : get_all_slash_commands()) {
                if (cmd_info.command.rfind(buffer, 0) == 0) {
                    live_matches.push_back(cmd_info);
                }
            }
            if (!live_matches.empty()) {
                if (popup_selected_idx >= static_cast<int>(live_matches.size())) {
                    popup_selected_idx = static_cast<int>(live_matches.size()) - 1;
                }
                size_t max_show = std::min(size_t(5), live_matches.size());
                for (size_t i = 0; i < max_show; ++i) {
                    std::cout << "\n\033[2K";
                    if (static_cast<int>(i) == popup_selected_idx) {
                        std::cout << "  " << ansi::BOLD_CYAN << "› " << ansi::BOLD_YELLOW << live_matches[i].command << ansi::RESET
                                  << "  " << ansi::BOLD_WHITE << live_matches[i].description << ansi::RESET;
                    } else {
                        std::cout << "    " << ansi::YELLOW << live_matches[i].command << ansi::RESET
                                  << "  " << ansi::DIM << live_matches[i].description << ansi::RESET;
                    }
                    lines_rendered_below++;
                }
                if (live_matches.size() > max_show) {
                    std::cout << "\n\033[2K  " << ansi::DIM << "... (" << (live_matches.size() - max_show) << " outros comandos disponíveis)" << ansi::RESET;
                    lines_rendered_below++;
                }
                if (lines_rendered_below > 0) {
                    std::cout << "\033[" << lines_rendered_below << "A";
                }
            }
        }

        // 3. Move cursor to right position on prompt line
        std::cout << "\r" << prompt << buffer.substr(0, cursor_pos) << std::flush;
    };

    redraw();

    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            clear_rendered_below();
            std::cout << "\n";
            return "/exit";
        }

        // Enter / Newline
        if (c == '\n' || c == '\r') {
            if (popup_selected_idx >= 0 && buffer.rfind("/", 0) == 0 && buffer.find(' ') == std::string::npos) {
                std::vector<SlashCommandInfo> matches;
                for (const auto& cmd_info : get_all_slash_commands()) {
                    if (cmd_info.command.rfind(buffer, 0) == 0) {
                        matches.push_back(cmd_info);
                    }
                }
                if (popup_selected_idx < static_cast<int>(matches.size())) {
                    buffer = matches[popup_selected_idx].command;
                }
            }
            clear_rendered_below();
            std::cout << "\n" << std::flush;
            return buffer;
        }

        // Backspace (ASCII 127 DEL or ASCII 8 BS / Ctrl+H)
        if (c == 127 || c == 8) {
            if (cursor_pos > 0) {
                size_t prev = cursor_pos - 1;
                while (prev > 0 && (static_cast<unsigned char>(buffer[prev]) & 0xC0) == 0x80) {
                    prev--;
                }
                size_t bytes_to_erase = cursor_pos - prev;
                buffer.erase(prev, bytes_to_erase);
                cursor_pos = prev;
                popup_selected_idx = -1;
                redraw();
            }
            continue;
        }

        // Tab Completion
        if (c == '\t') {
            if (buffer.rfind("/", 0) == 0) {
                auto space_pos = buffer.find(' ');
                if (space_pos == std::string::npos) {
                    std::string prefix = buffer.substr(0, cursor_pos);
                    std::string trailing = buffer.substr(cursor_pos);
                    std::vector<SlashCommandInfo> matches;
                    for (const auto& cmd_info : get_all_slash_commands()) {
                        if (cmd_info.command.rfind(prefix, 0) == 0) {
                            matches.push_back(cmd_info);
                        }
                    }
                    if (popup_selected_idx >= 0 && popup_selected_idx < static_cast<int>(matches.size())) {
                        buffer = matches[popup_selected_idx].command + " " + trailing;
                        cursor_pos = matches[popup_selected_idx].command.size() + 1;
                        popup_selected_idx = -1;
                        redraw();
                    } else if (matches.size() == 1) {
                        buffer = matches[0].command + " " + trailing;
                        cursor_pos = matches[0].command.size() + 1;
                        popup_selected_idx = -1;
                        redraw();
                    } else if (matches.size() > 1) {
                        std::vector<std::string> names;
                        for (const auto& m : matches) names.push_back(m.command);
                        std::string common = compute_longest_common_prefix(names);
                        if (common.size() > prefix.size()) {
                            buffer = common + trailing;
                            cursor_pos = common.size();
                        } else {
                            popup_selected_idx = (popup_selected_idx + 1) % static_cast<int>(matches.size());
                        }
                        redraw();
                    }
                } else {
                    std::string raw_cmd = buffer.substr(0, space_pos);
                    std::string arg = buffer.substr(space_pos + 1, cursor_pos > space_pos ? cursor_pos - (space_pos + 1) : 0);
                    std::string trailing = buffer.substr(cursor_pos);

                    auto [resolved_opt, _] = resolve_slash_command(raw_cmd, !arg.empty());
                    std::string cmd = resolved_opt.value_or(raw_cmd);

                    if (cmd == "/add" || cmd == "/drop" || cmd == "/gentest" || cmd == "/gentests" || cmd == "/export") {
                        auto files = complete_files(arg, ".");
                        if (files.size() == 1) {
                            buffer = raw_cmd + " " + files[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + files[0].size();
                            redraw();
                        } else if (!files.empty()) {
                            std::string common = compute_longest_common_prefix(files);
                            if (common.size() > arg.size()) {
                                buffer = raw_cmd + " " + common + trailing;
                                cursor_pos = raw_cmd.size() + 1 + common.size();
                            }
                            print_completion_grid(files, "Arquivos correspondentes:");
                            redraw();
                        }
                    } else if (cmd == "/model" || cmd == "/architect" || cmd == "/arch") {
                        auto models = complete_models(arg);
                        if (models.size() == 1) {
                            buffer = raw_cmd + " " + models[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + models[0].size();
                            redraw();
                        } else if (!models.empty()) {
                            std::string common = compute_longest_common_prefix(models);
                            if (common.size() > arg.size()) {
                                buffer = raw_cmd + " " + common + trailing;
                                cursor_pos = raw_cmd.size() + 1 + common.size();
                            }
                            print_completion_grid(models, "Modelos disponíveis:");
                            redraw();
                        }
                    } else if (cmd == "/lang" || cmd == "/language") {
                        auto langs = complete_languages(arg);
                        if (langs.size() == 1) {
                            buffer = raw_cmd + " " + langs[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + langs[0].size();
                            redraw();
                        } else if (!langs.empty()) {
                            std::string common = compute_longest_common_prefix(langs);
                            if (common.size() > arg.size()) {
                                buffer = raw_cmd + " " + common + trailing;
                                cursor_pos = raw_cmd.size() + 1 + common.size();
                            }
                            print_completion_grid(langs, "Idiomas disponíveis:");
                            redraw();
                        }
                    } else if (cmd == "/key" || cmd == "/setkey") {
                        auto keys = complete_keys(arg);
                        if (keys.size() == 1) {
                            buffer = raw_cmd + " " + keys[0] + " " + trailing;
                            cursor_pos = raw_cmd.size() + 1 + keys[0].size() + 1;
                            redraw();
                        } else if (!keys.empty()) {
                            std::string common = compute_longest_common_prefix(keys);
                            if (common.size() > arg.size()) {
                                buffer = raw_cmd + " " + common + trailing;
                                cursor_pos = raw_cmd.size() + 1 + common.size();
                            }
                            print_completion_grid(keys, "Chaves / Variáveis disponíveis:");
                            redraw();
                        }
                    } else if (cmd == "/repomap") {
                        std::vector<std::string> sub = {"on", "off", "30", "60", "100"};
                        std::vector<std::string> matched_sub;
                        for (const auto& s : sub) {
                            if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                        }
                        if (matched_sub.size() == 1) {
                            buffer = raw_cmd + " " + matched_sub[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                            redraw();
                        } else if (!matched_sub.empty()) {
                            print_completion_grid(matched_sub, "Opções RepoMap:");
                            redraw();
                        }
                    } else if (cmd == "/session") {
                        if (arg.rfind("load ", 0) == 0 || arg.rfind("delete ", 0) == 0 || arg.rfind("del ", 0) == 0) {
                            size_t sub_sp = arg.find(' ');
                            std::string sub_action = arg.substr(0, sub_sp + 1);
                            std::string s_prefix = arg.substr(sub_sp + 1);

                            std::vector<std::string> session_names;
                            Config& cfg = get_config();
                            std::filesystem::path s_dir = cfg.project_root / ".llmcli" / "sessions";
                            if (std::filesystem::exists(s_dir)) {
                                for (const auto& e : std::filesystem::directory_iterator(s_dir)) {
                                    if (e.path().extension() == ".json") session_names.push_back(e.path().stem().string());
                                }
                            }
                            const char* home = std::getenv("HOME");
                            if (home) {
                                std::filesystem::path h_dir = std::filesystem::path(home) / ".llmcli_sessions";
                                if (std::filesystem::exists(h_dir)) {
                                    for (const auto& e : std::filesystem::directory_iterator(h_dir)) {
                                        if (e.path().extension() == ".json") session_names.push_back(e.path().stem().string());
                                    }
                                }
                            }
                            std::vector<std::string> matches_s;
                            for (const auto& sn : session_names) {
                                if (sn.rfind(s_prefix, 0) == 0) matches_s.push_back(sn);
                            }
                            if (matches_s.size() == 1) {
                                buffer = raw_cmd + " " + sub_action + matches_s[0] + trailing;
                                cursor_pos = raw_cmd.size() + 1 + sub_action.size() + matches_s[0].size();
                                redraw();
                            } else if (!matches_s.empty()) {
                                std::string common = compute_longest_common_prefix(matches_s);
                                if (common.size() > s_prefix.size()) {
                                    buffer = raw_cmd + " " + sub_action + common + trailing;
                                    cursor_pos = raw_cmd.size() + 1 + sub_action.size() + common.size();
                                }
                                print_completion_grid(matches_s, "Sessões salvas disponíveis:");
                                redraw();
                            }
                        } else {
                            std::vector<std::string> sub = {"list", "save ", "load ", "delete "};
                            std::vector<std::string> matched_sub;
                            for (const auto& s : sub) {
                                if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                            }
                            if (matched_sub.size() == 1) {
                                buffer = raw_cmd + " " + matched_sub[0] + trailing;
                                cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                                redraw();
                            } else if (!matched_sub.empty()) {
                                print_completion_grid(matched_sub, "Comandos /session:");
                                redraw();
                            }
                        }
                    } else if (cmd == "/todo") {
                        std::vector<std::string> sub = {"add ", "check ", "done ", "clear"};
                        std::vector<std::string> matched_sub;
                        for (const auto& s : sub) {
                            if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                        }
                        if (matched_sub.size() == 1) {
                            buffer = raw_cmd + " " + matched_sub[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                            redraw();
                        } else if (!matched_sub.empty()) {
                            print_completion_grid(matched_sub, "Comandos /todo:");
                            redraw();
                        }
                    } else if (cmd == "/test") {
                        std::vector<std::string> sub = {"make test", "make", "ctest"};
                        std::vector<std::string> matched_sub;
                        for (const auto& s : sub) {
                            if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                        }
                        if (matched_sub.size() == 1) {
                            buffer = raw_cmd + " " + matched_sub[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                            redraw();
                        } else if (!matched_sub.empty()) {
                            print_completion_grid(matched_sub, "Comandos de Teste:");
                            redraw();
                        }
                    } else if (cmd == "/scan" || cmd == "/host") {
                        std::vector<std::string> sub = {"127.0.0.1", "localhost", "192.168.0."};
                        std::vector<std::string> matched_sub;
                        for (const auto& s : sub) {
                            if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                        }
                        if (matched_sub.size() == 1) {
                            buffer = raw_cmd + " " + matched_sub[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                            redraw();
                        } else if (!matched_sub.empty()) {
                            print_completion_grid(matched_sub, "Hosts / IPs sugeridos:");
                            redraw();
                        }
                    } else if (cmd == "/reset") {
                        std::vector<std::string> sub = {"prefs", "all"};
                        std::vector<std::string> matched_sub;
                        for (const auto& s : sub) {
                            if (s.rfind(arg, 0) == 0) matched_sub.push_back(s);
                        }
                        if (matched_sub.size() == 1) {
                            buffer = raw_cmd + " " + matched_sub[0] + trailing;
                            cursor_pos = raw_cmd.size() + 1 + matched_sub[0].size();
                            redraw();
                        } else if (!matched_sub.empty()) {
                            print_completion_grid(matched_sub, "Opções de /reset:");
                            redraw();
                        }
                    }
                }
            } else {
                // Subprompt model completion (e.g. inside /model interactive picker)
                if (prompt.find("modelo") != std::string::npos || prompt.find("model") != std::string::npos || prompt.find("Escolha") != std::string::npos) {
                    std::string current_word = buffer.substr(0, cursor_pos);
                    std::string trailing = buffer.substr(cursor_pos);
                    auto models = complete_models(current_word);
                    if (models.size() == 1) {
                        buffer = models[0] + trailing;
                        cursor_pos = models[0].size();
                        redraw();
                    } else if (!models.empty()) {
                        std::string common = compute_longest_common_prefix(models);
                        if (common.size() > current_word.size()) {
                            buffer = common + trailing;
                            cursor_pos = common.size();
                        }
                        print_completion_grid(models, "Modelos disponíveis:");
                        redraw();
                    }
                } else {
                    // Check for @mention file completion in normal prompt
                    size_t at_pos = buffer.rfind('@', cursor_pos > 0 ? cursor_pos - 1 : 0);
                    if (at_pos != std::string::npos && (at_pos == 0 || buffer[at_pos - 1] == ' ' || buffer[at_pos - 1] == '\t')) {
                        std::string file_prefix = buffer.substr(at_pos + 1, cursor_pos - (at_pos + 1));
                        std::string trailing = buffer.substr(cursor_pos);
                        auto files = complete_files(file_prefix, ".");
                        if (files.size() == 1) {
                            buffer = buffer.substr(0, at_pos + 1) + files[0] + trailing;
                            cursor_pos = at_pos + 1 + files[0].size();
                            redraw();
                        } else if (!files.empty()) {
                            std::string common = compute_longest_common_prefix(files);
                            if (common.size() > file_prefix.size()) {
                                buffer = buffer.substr(0, at_pos + 1) + common + trailing;
                                cursor_pos = at_pos + 1 + common.size();
                            }
                            print_completion_grid(files, "Arquivos correspondentes (@):");
                            redraw();
                        }
                    }
                }
            }
            continue;
        }

        // Ctrl+C (cancel current input line)
        if (c == 3) {
            std::cout << "^C\n";
            buffer.clear();
            cursor_pos = 0;
            redraw();
            continue;
        }

        // Ctrl+D (EOF on empty buffer, delete at cursor otherwise)
        if (c == 4) {
            if (buffer.empty()) {
                std::cout << "\n";
                return "/exit";
            }
            if (cursor_pos < buffer.size()) {
                size_t next = cursor_pos + 1;
                while (next < buffer.size() && (static_cast<unsigned char>(buffer[next]) & 0xC0) == 0x80) {
                    next++;
                }
                buffer.erase(cursor_pos, next - cursor_pos);
                redraw();
            }
            continue;
        }

        // Ctrl+A (Home)
        if (c == 1) {
            cursor_pos = 0;
            redraw();
            continue;
        }

        // Ctrl+E (End)
        if (c == 5) {
            cursor_pos = buffer.size();
            redraw();
            continue;
        }

        // Ctrl+U (Clear line before cursor)
        if (c == 21) {
            buffer.erase(0, cursor_pos);
            cursor_pos = 0;
            redraw();
            continue;
        }

        // Ctrl+K (Clear line after cursor)
        if (c == 11) {
            buffer.erase(cursor_pos);
            redraw();
            continue;
        }

        // Ctrl+W (Delete previous word)
        if (c == 23) {
            if (cursor_pos > 0) {
                size_t p = cursor_pos;
                while (p > 0 && buffer[p - 1] == ' ') p--;
                while (p > 0 && buffer[p - 1] != ' ') p--;
                buffer.erase(p, cursor_pos - p);
                cursor_pos = p;
                redraw();
            }
            continue;
        }

        // Ctrl+L (Clear screen)
        if (c == 12) {
            std::cout << "\033[2J\033[H";
            redraw();
            continue;
        }

        // Ctrl+R (Reverse history search)
        if (c == 18) {
            clear_rendered_below();
            std::string search_query;
            int found_idx = -1;

            auto search_history = [&](const std::string& q, int start_from) -> int {
                if (q.empty()) return -1;
                for (int i = start_from; i >= 0; --i) {
                    if (i < static_cast<int>(history_.size()) && history_[i].find(q) != std::string::npos) {
                        return i;
                    }
                }
                return -1;
            };

            while (true) {
                std::string match_str = (found_idx >= 0 && found_idx < static_cast<int>(history_.size()))
                                        ? history_[found_idx] : "";
                std::cout << "\r\033[2K" << ansi::BOLD_CYAN << "(reverse-i-search)`"
                          << ansi::BOLD_YELLOW << search_query << ansi::BOLD_CYAN << "': "
                          << ansi::RESET << match_str << std::flush;

                char rc;
                if (read(STDIN_FILENO, &rc, 1) <= 0) break;

                if (rc == 18) { // Ctrl+R -> search further back
                    if (found_idx > 0) {
                        int next_found = search_history(search_query, found_idx - 1);
                        if (next_found >= 0) found_idx = next_found;
                    }
                } else if (rc == 127 || rc == 8) { // Backspace
                    if (!search_query.empty()) {
                        search_query.pop_back();
                        found_idx = search_history(search_query, static_cast<int>(history_.size()) - 1);
                    }
                } else if (rc == 27 || rc == 7 || rc == 3) { // ESC, Ctrl+G, Ctrl+C -> cancel
                    break;
                } else if (rc == '\n' || rc == '\r') { // Enter -> submit matched command
                    if (found_idx >= 0 && found_idx < static_cast<int>(history_.size())) {
                        buffer = history_[found_idx];
                        cursor_pos = buffer.size();
                    }
                    std::cout << "\n";
                    return buffer;
                } else if (static_cast<unsigned char>(rc) >= 32) { // Character input
                    search_query.push_back(rc);
                    found_idx = search_history(search_query, static_cast<int>(history_.size()) - 1);
                } else {
                    // Control key -> put matched text into buffer and resume editing
                    if (found_idx >= 0 && found_idx < static_cast<int>(history_.size())) {
                        buffer = history_[found_idx];
                        cursor_pos = buffer.size();
                    }
                    break;
                }
            }
            redraw();
            continue;
        }

        // Escape Sequence (Arrows, Home, End, Delete)
        if (c == '\033') {
            char seq[5];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    char seq2;
                    if (read(STDIN_FILENO, &seq2, 1) <= 0) continue;
                    if (seq[1] == '3' && seq2 == '~') {
                        // Delete key
                        if (cursor_pos < buffer.size()) {
                            size_t next = cursor_pos + 1;
                            while (next < buffer.size() && (static_cast<unsigned char>(buffer[next]) & 0xC0) == 0x80) {
                                next++;
                            }
                            buffer.erase(cursor_pos, next - cursor_pos);
                            redraw();
                        }
                    } else if (seq[1] == '1' && seq2 == '~') {
                        // Home
                        cursor_pos = 0;
                        redraw();
                    } else if (seq[1] == '4' && seq2 == '~') {
                        // End
                        cursor_pos = buffer.size();
                        redraw();
                    }
                } else {
                    switch (seq[1]) {
                        case 'A': // UP
                            if (buffer.rfind("/", 0) == 0 && buffer.find(' ') == std::string::npos) {
                                std::vector<SlashCommandInfo> matches;
                                for (const auto& cmd_info : get_all_slash_commands()) {
                                    if (cmd_info.command.rfind(buffer, 0) == 0) {
                                        matches.push_back(cmd_info);
                                    }
                                }
                                if (!matches.empty()) {
                                    if (popup_selected_idx <= 0) popup_selected_idx = static_cast<int>(matches.size()) - 1;
                                    else popup_selected_idx--;
                                    redraw();
                                    break;
                                }
                            }
                            if (!history_.empty()) {
                                if (history_idx == static_cast<int>(history_.size())) {
                                    saved_buf = buffer;
                                }
                                if (history_idx > 0) {
                                    history_idx--;
                                    buffer = history_[history_idx];
                                    cursor_pos = buffer.size();
                                    popup_selected_idx = -1;
                                    redraw();
                                }
                            }
                            break;
                        case 'B': // DOWN
                            if (buffer.rfind("/", 0) == 0 && buffer.find(' ') == std::string::npos) {
                                std::vector<SlashCommandInfo> matches;
                                for (const auto& cmd_info : get_all_slash_commands()) {
                                    if (cmd_info.command.rfind(buffer, 0) == 0) {
                                        matches.push_back(cmd_info);
                                    }
                                }
                                if (!matches.empty()) {
                                    if (popup_selected_idx >= static_cast<int>(matches.size()) - 1) popup_selected_idx = 0;
                                    else popup_selected_idx++;
                                    redraw();
                                    break;
                                }
                            }
                            if (!history_.empty()) {
                                if (history_idx < static_cast<int>(history_.size()) - 1) {
                                    history_idx++;
                                    buffer = history_[history_idx];
                                    cursor_pos = buffer.size();
                                    popup_selected_idx = -1;
                                    redraw();
                                } else if (history_idx == static_cast<int>(history_.size()) - 1) {
                                    history_idx++;
                                    buffer = saved_buf;
                                    cursor_pos = buffer.size();
                                    popup_selected_idx = -1;
                                    redraw();
                                }
                            }
                            break;
                        case 'C': // RIGHT
                            if (cursor_pos < buffer.size()) {
                                size_t next = cursor_pos + 1;
                                while (next < buffer.size() && (static_cast<unsigned char>(buffer[next]) & 0xC0) == 0x80) {
                                    next++;
                                }
                                cursor_pos = next;
                                redraw();
                            }
                            break;
                        case 'D': // LEFT
                            if (cursor_pos > 0) {
                                size_t prev = cursor_pos - 1;
                                while (prev > 0 && (static_cast<unsigned char>(buffer[prev]) & 0xC0) == 0x80) {
                                    prev--;
                                }
                                cursor_pos = prev;
                                redraw();
                            }
                            break;
                        case 'H': // HOME
                            cursor_pos = 0;
                            redraw();
                            break;
                        case 'F': // END
                            cursor_pos = buffer.size();
                            redraw();
                            break;
                    }
                }
            } else if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H': // HOME
                        cursor_pos = 0;
                        redraw();
                        break;
                    case 'F': // END
                        cursor_pos = buffer.size();
                        redraw();
                        break;
                }
            }
            continue;
        }

        // Regular or UTF-8 character insertion
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc != 127) {
            size_t utf8_len = 1;
            if ((uc & 0xE0) == 0xC0) utf8_len = 2;
            else if ((uc & 0xF0) == 0xE0) utf8_len = 3;
            else if ((uc & 0xF8) == 0xF0) utf8_len = 4;

            std::string char_bytes;
            char_bytes.push_back(c);
            for (size_t i = 1; i < utf8_len; ++i) {
                char next_c;
                if (read(STDIN_FILENO, &next_c, 1) > 0) {
                    char_bytes.push_back(next_c);
                }
            }

            buffer.insert(cursor_pos, char_bytes);
            cursor_pos += char_bytes.size();
            popup_selected_idx = -1;
            redraw();
        }
    }
}

bool ReplSession::handle_slash_command(const std::string& cmd_line) {
    std::string trimmed = cmd_line;
    auto f = trimmed.find_first_not_of(" \t\r\n");
    auto l = trimmed.find_last_not_of(" \t\r\n");
    if (f == std::string::npos) return true;
    trimmed = trimmed.substr(f, l - f + 1);

    std::string raw_command;
    std::string arg;
    auto space_pos = trimmed.find(' ');
    if (space_pos != std::string::npos) {
        raw_command = trimmed.substr(0, space_pos);
        arg = trimmed.substr(space_pos + 1);
        auto af = arg.find_first_not_of(" \t\r\n");
        auto al = arg.find_last_not_of(" \t\r\n");
        if (af != std::string::npos) {
            arg = arg.substr(af, al - af + 1);
        } else {
            arg = "";
        }
    } else {
        raw_command = trimmed;
    }

    auto [resolved_cmd, matches] = resolve_slash_command(raw_command, !arg.empty());

    if (!resolved_cmd.has_value()) {
        if (!matches.empty()) {
            std::cout << ansi::YELLOW << "Comando ambíguo '" << raw_command << "'. Opções possíveis: ";
            for (size_t i = 0; i < matches.size(); ++i) {
                std::cout << matches[i] << (i + 1 < matches.size() ? ", " : "\n");
            }
            std::cout << ansi::RESET;
        } else {
            std::cout << ansi::RED << "Comando desconhecido: '" << raw_command << "'. Digite /help para ver os comandos disponíveis." << ansi::RESET << "\n";
        }
        return true;
    }

    std::string command = resolved_cmd.value();
    Config& cfg = get_config();
    UserPreferences& prefs = get_preferences();

    if (command == "/exit" || command == "/quit" || command == "/q") {
        std::cout << ansi::CYAN << i18n::t("exit_msg") << ansi::RESET << "\n";
        return false;
    }

    if (command == "/yolo") {
        cfg.yolo_mode = !cfg.yolo_mode;
        prefs.set_model_pref(cfg.active_model, "yolo_mode", cfg.yolo_mode);
        prefs.set_global_pref("yolo_mode", cfg.yolo_mode);
        if (cfg.yolo_mode) {
            std::cout << ansi::BOLD_RED << i18n::t("yolo_on", {{"model", cfg.active_model}}) << ansi::RESET << "\n";
        } else {
            std::cout << ansi::BOLD_GREEN << i18n::t("yolo_off", {{"model", cfg.active_model}}) << ansi::RESET << "\n";
        }
    }
    else if (command == "/architect" || command == "/arch") {
        if (arg.empty()) {
            cfg.architect_mode = !cfg.architect_mode;
            prefs.set_global_pref("architect_mode", cfg.architect_mode);
            if (cfg.architect_mode) {
                std::cout << ansi::BOLD_MAGENTA << i18n::t("arch_on", {{"arch", cfg.architect_model}, {"editor", cfg.active_model}}) << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_GREEN << i18n::t("arch_off") << ansi::RESET << "\n";
            }
        } else if (arg == "off" || arg == "disable" || arg == "desativar" || arg == "false") {
            cfg.architect_mode = false;
            prefs.set_global_pref("architect_mode", false);
            std::cout << ansi::BOLD_GREEN << "🛡️ MODO ARQUITETO DESATIVADO." << ansi::RESET << "\n";
        } else {
            cfg.architect_mode = true;
            cfg.architect_model = arg;
            prefs.set_global_pref("architect_mode", true);
            prefs.set_global_pref("architect_model", arg);
            std::cout << ansi::BOLD_MAGENTA << i18n::t("arch_on", {{"arch", arg}, {"editor", cfg.active_model}}) << ansi::RESET << "\n";
        }
    }
    else if (command == "/dryrun" || command == "/dry-run") {
        cfg.dry_run = !cfg.dry_run;
        if (cfg.dry_run) {
            std::cout << ansi::BOLD_CYAN << i18n::t("dryrun_on") << ansi::RESET << "\n";
        } else {
            std::cout << ansi::BOLD_GREEN << i18n::t("dryrun_off") << ansi::RESET << "\n";
        }
    }
    else if (command == "/lang" || command == "/language") {
        if (arg.empty()) {
            std::string cur_code = i18n::get_active_language();
            auto supp = i18n::get_supported_languages();
            auto it = supp.find(cur_code);
            std::string name = (it != supp.end()) ? it->second.name : cur_code;
            std::string flag = (it != supp.end()) ? it->second.flag : "🌐";

            std::cout << ansi::BOLD_CYAN << i18n::t("lang_current", {{"flag", flag}, {"name", name}, {"code", cur_code}}) << ansi::RESET << "\n\n";
            std::cout << ansi::DIM << "Idiomas suportados / Supported languages:" << ansi::RESET << "\n";
            for (const auto& [code, info] : supp) {
                std::string cur_marker = (code == cur_code) ? std::string(ansi::BOLD_GREEN) + " ✓" + ansi::RESET : "";
                std::cout << "  " << info.flag << " " << ansi::BOLD_YELLOW << code << ansi::RESET << " - " << info.name << cur_marker << "\n";
            }
            std::cout << "  🌐 " << ansi::BOLD_YELLOW << "auto" << ansi::RESET << " - Detecção automática (SO / OS locale)\n\n";
            std::cout << ansi::DIM << "Use '/lang <código>' (ex: /lang en, /lang pt, /lang es, /lang de, /lang fr, /lang zh, /lang ru, /lang hi, /lang auto)" << ansi::RESET << "\n";
        } else {
            std::string resolved = i18n::set_active_language(arg);
            prefs.set_global_pref("language", resolved);
            cfg.language = resolved;
            auto supp = i18n::get_supported_languages();
            auto it = supp.find(resolved);
            std::string name = (it != supp.end()) ? it->second.name : resolved;
            std::string flag = (it != supp.end()) ? it->second.flag : "🌐";
            std::cout << ansi::BOLD_GREEN << i18n::t("lang_changed", {{"flag", flag}, {"name", name}, {"code", resolved}}) << ansi::RESET << "\n";
        }
    }
    else if (command == "/model") {
        if (arg.empty()) {
            auto presets = providers::ProviderRegistry::get_model_presets();
            std::vector<std::string> headers = {"#", "Categoria", "Nome do Modelo", "Descrição", "Status"};
            std::vector<std::vector<std::string>> rows;

            for (const auto& p : presets) {
                bool is_active = (p.name == cfg.active_model);
                std::string status_badge = is_active ? std::string(ansi::BOLD_GREEN) + "✓ ATIVO" + ansi::RESET : "";
                std::string model_style = is_active ? std::string(ansi::BOLD_GREEN) + p.name + ansi::RESET
                                                    : std::string(ansi::BOLD_WHITE) + p.name + ansi::RESET;
                rows.push_back({
                    std::to_string(p.id),
                    p.category,
                    model_style,
                    p.desc,
                    status_badge
                });
            }
            ansi::print_table(headers, rows, "🤖 Modelos Disponíveis no llmCli");
            std::cout << ansi::DIM << "Modelo ativo: " << ansi::BOLD_YELLOW << cfg.active_model << ansi::RESET << " (Temp: " << cfg.temperature << ")\n";

            std::string sub_prompt = std::string(ansi::CYAN) + "Escolha o modelo (digite o número ou nome) ❯ " + ansi::RESET;
            std::string chosen = read_line_input(sub_prompt);
            auto cf = chosen.find_first_not_of(" \t\r\n");
            auto cl = chosen.find_last_not_of(" \t\r\n");
            if (cf != std::string::npos) {
                chosen = chosen.substr(cf, cl - cf + 1);
            } else {
                chosen = "";
            }
            if (!chosen.empty() && chosen != "/exit") {
                std::string res_model = providers::ProviderRegistry::resolve_model_by_id_or_name(chosen);
                agent_->set_model(res_model);
                prefs.set_global_pref("last_active_model", res_model);
                std::cout << ansi::BOLD_GREEN << i18n::t("model_switched", {{"model", res_model}}) << ansi::RESET << "\n";
            }
        } else {
            std::string res_model = providers::ProviderRegistry::resolve_model_by_id_or_name(arg);
            agent_->set_model(res_model);
            prefs.set_global_pref("last_active_model", res_model);
            std::cout << ansi::BOLD_GREEN << i18n::t("model_switched", {{"model", res_model}}) << ansi::RESET << "\n";
        }
    }
    else if (command == "/models") {
        std::cout << ansi::DIM << "Verificando status de conectividade dos provedores..." << ansi::RESET << "\n";
        auto status = providers::ProviderRegistry::get_status_overview();
        print_status_table(status);
    }
    else if (command == "/key" || command == "/setkey") {
        if (arg.empty()) {
            std::cout << ansi::BOLD_CYAN << "🔑 Chaves e Endpoints de API Configurados:" << ansi::RESET << "\n";
            std::vector<std::pair<std::string, std::string>> keys = {
                {"GEMINI_API_KEY", utils::EnvLoader::get_env("GEMINI_API_KEY")},
                {"ANTHROPIC_API_KEY", utils::EnvLoader::get_env("ANTHROPIC_API_KEY")},
                {"OPENAI_API_KEY", utils::EnvLoader::get_env("OPENAI_API_KEY")},
                {"GROQ_API_KEY", utils::EnvLoader::get_env("GROQ_API_KEY")},
                {"DEEPSEEK_API_KEY", utils::EnvLoader::get_env("DEEPSEEK_API_KEY")},
                {"TAVILY_API_KEY", utils::EnvLoader::get_env("TAVILY_API_KEY")},
                {"LLAMACPP_BASE_URL", utils::EnvLoader::get_env("LLAMACPP_BASE_URL", cfg.local_endpoints.llamacpp)},
                {"OLLAMA_BASE_URL", utils::EnvLoader::get_env("OLLAMA_BASE_URL", cfg.local_endpoints.ollama)}
            };
            for (const auto& [k, v] : keys) {
                if (v.empty()) {
                    std::cout << "  " << ansi::BOLD_YELLOW << k << ansi::RESET << ": " << ansi::RED << "(não configurada)" << ansi::RESET << "\n";
                } else {
                    std::string masked = (v.size() > 8 && k.find("URL") == std::string::npos)
                                        ? v.substr(0, 4) + "..." + v.substr(v.size() - 4)
                                        : v;
                    std::cout << "  " << ansi::BOLD_YELLOW << k << ansi::RESET << ": " << ansi::BOLD_GREEN << masked << ansi::RESET << "\n";
                }
            }
            std::cout << "\n" << ansi::DIM << "Para configurar: /key <NOME_DA_VARIAVEL> <VALOR> (ex: /key GEMINI_API_KEY AIzaSy...)" << ansi::RESET << "\n";
        } else {
            auto space_p = arg.find(' ');
            if (space_p == std::string::npos) {
                std::string k = arg;
                if (k.find("_API_KEY") == std::string::npos && k.find("URL") == std::string::npos) {
                    if (cfg.active_model.rfind("gemini", 0) == 0) k = "GEMINI_API_KEY";
                    else if (cfg.active_model.rfind("claude", 0) == 0 || cfg.active_model.rfind("anthropic", 0) == 0) k = "ANTHROPIC_API_KEY";
                    else if (cfg.active_model.rfind("openai", 0) == 0 || cfg.active_model.rfind("gpt", 0) == 0) k = "OPENAI_API_KEY";
                    else if (cfg.active_model.rfind("groq", 0) == 0) k = "GROQ_API_KEY";
                    else if (cfg.active_model.rfind("deepseek", 0) == 0) k = "DEEPSEEK_API_KEY";
                    else k = "GEMINI_API_KEY";

                    std::filesystem::path dot_env = cfg.project_root / ".env";
                    utils::EnvLoader::save_env_var(k, arg, dot_env);
                    agent_->refresh_provider();
                    std::cout << ansi::BOLD_GREEN << "✓ Chave " << k << " configurada e salva em " << dot_env.string() << "!" << ansi::RESET << "\n";
                } else {
                    std::cout << ansi::YELLOW << "Uso: /key <NOME_DA_VARIAVEL> <VALOR>" << ansi::RESET << "\n";
                }
            } else {
                std::string k = arg.substr(0, space_p);
                std::string v = arg.substr(space_p + 1);
                auto vf = v.find_first_not_of(" \t\r\n");
                auto vl = v.find_last_not_of(" \t\r\n");
                if (vf != std::string::npos) v = v.substr(vf, vl - vf + 1);

                if (k == "gemini") k = "GEMINI_API_KEY";
                else if (k == "anthropic" || k == "claude") k = "ANTHROPIC_API_KEY";
                else if (k == "openai" || k == "gpt") k = "OPENAI_API_KEY";
                else if (k == "groq") k = "GROQ_API_KEY";
                else if (k == "deepseek") k = "DEEPSEEK_API_KEY";
                else if (k == "tavily") k = "TAVILY_API_KEY";

                std::filesystem::path dot_env = cfg.project_root / ".env";
                utils::EnvLoader::save_env_var(k, v, dot_env);
                agent_->refresh_provider();
                std::cout << ansi::BOLD_GREEN << "✓ Chave " << k << " configurada e salva em " << dot_env.string() << "!" << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/repomap") {
        if (arg.empty()) {
            std::string status = cfg.enable_repomap ? (std::string(ansi::BOLD_GREEN) + "ATIVADO" + ansi::RESET)
                                                    : (std::string(ansi::BOLD_RED) + "DESATIVADO" + ansi::RESET);
            std::cout << ansi::BOLD_CYAN << "🗺️ Status do RepoMap: " << status << ansi::RESET << "\n"
                      << "  Limite máximo de arquivos mapeados: " << ansi::BOLD_YELLOW << cfg.repomap_max_files << ansi::RESET << "\n"
                      << ansi::DIM << "Use: /repomap on, /repomap off, /repomap <número_de_arquivos> (ex: /repomap 30)" << ansi::RESET << "\n";
        } else if (arg == "on" || arg == "enable" || arg == "ativar" || arg == "true") {
            cfg.enable_repomap = true;
            prefs.set_global_pref("enable_repomap", true);
            std::cout << ansi::BOLD_GREEN << "✓ RepoMap ativado no contexto do sistema." << ansi::RESET << "\n";
        } else if (arg == "off" || arg == "disable" || arg == "desativar" || arg == "false") {
            cfg.enable_repomap = false;
            prefs.set_global_pref("enable_repomap", false);
            std::cout << ansi::BOLD_GREEN << "✓ RepoMap desativado (economia de ~400 a ~500 tokens de prompt)." << ansi::RESET << "\n";
        } else {
            try {
                int limit = std::stoi(arg);
                if (limit > 0 && limit <= 500) {
                    cfg.repomap_max_files = limit;
                    cfg.enable_repomap = true;
                    prefs.set_global_pref("enable_repomap", true);
                    prefs.set_global_pref("repomap_max_files", limit);
                    std::cout << ansi::BOLD_GREEN << "✓ Limite do RepoMap definido para " << limit << " arquivos." << ansi::RESET << "\n";
                } else {
                    std::cout << ansi::RED << "Por favor, especifique um valor entre 1 e 500." << ansi::RESET << "\n";
                }
            } catch (...) {
                std::cout << ansi::RED << "Uso: /repomap on, /repomap off, /repomap <número>" << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/session") {
        std::filesystem::path sessions_dir = cfg.project_root / ".llmcli" / "sessions";
        const char* home = std::getenv("HOME");
        std::filesystem::path home_sessions_dir;
        if (home) home_sessions_dir = std::filesystem::path(home) / ".llmcli_sessions";

        auto get_session_file = [&](const std::string& name) -> std::filesystem::path {
            std::string sanitized = name;
            for (char& c : sanitized) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
            }
            if (sanitized.rfind(".json") == std::string::npos) sanitized += ".json";
            return sessions_dir / sanitized;
        };

        if (arg.empty() || arg == "list") {
            std::vector<std::filesystem::path> all_files;
            if (std::filesystem::exists(sessions_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(sessions_dir)) {
                    if (entry.path().extension() == ".json") all_files.push_back(entry.path());
                }
            }
            if (!home_sessions_dir.empty() && std::filesystem::exists(home_sessions_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(home_sessions_dir)) {
                    if (entry.path().extension() == ".json") all_files.push_back(entry.path());
                }
            }

            if (all_files.empty()) {
                std::cout << ansi::DIM << "Nenhuma sessão salva encontrada. Salve com '/session save <nome>'." << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_CYAN << "📁 Sessões Salvas Disponíveis (" << all_files.size() << "):" << ansi::RESET << "\n";
                for (const auto& sf : all_files) {
                    std::string s_name = sf.stem().string();
                    std::cout << "  💾 " << ansi::BOLD_YELLOW << s_name << ansi::RESET << " (" << sf.string() << ")\n";
                }
                std::cout << "\n" << ansi::DIM << "Para carregar: /session load <nome>" << ansi::RESET << "\n";
            }
        } else if (arg.rfind("save ", 0) == 0) {
            std::string s_name = arg.substr(5);
            auto sf = get_session_file(s_name);
            if (agent_->session().save_to_file(sf)) {
                std::cout << ansi::BOLD_GREEN << "✓ Sessão salva com sucesso em: " << ansi::BOLD_YELLOW << sf.string() << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_RED << "Falha ao salvar sessão." << ansi::RESET << "\n";
            }
        } else if (arg.rfind("load ", 0) == 0) {
            std::string s_name = arg.substr(5);
            auto sf = get_session_file(s_name);
            if (!std::filesystem::exists(sf) && !home_sessions_dir.empty()) {
                auto h_sf = home_sessions_dir / (s_name + ".json");
                if (std::filesystem::exists(h_sf)) sf = h_sf;
            }
            if (agent_->session().load_from_file(sf)) {
                std::cout << ansi::BOLD_GREEN << "✓ Sessão '" << s_name << "' carregada com sucesso! ("
                          << agent_->session().messages().size() << " mensagens restauradas)" << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_RED << "Sessão '" << s_name << "' não encontrada ou corrompida." << ansi::RESET << "\n";
            }
        } else if (arg.rfind("delete ", 0) == 0 || arg.rfind("del ", 0) == 0) {
            auto sp = arg.find(' ');
            std::string s_name = arg.substr(sp + 1);
            auto sf = get_session_file(s_name);
            if (std::filesystem::exists(sf) && std::filesystem::remove(sf)) {
                std::cout << ansi::BOLD_GREEN << "✓ Sessão '" << s_name << "' removida com sucesso." << ansi::RESET << "\n";
            } else {
                std::cout << ansi::RED << "Sessão '" << s_name << "' não encontrada." << ansi::RESET << "\n";
            }
        } else {
            std::cout << ansi::DIM << "Uso: /session list, /session save <nome>, /session load <nome>, /session delete <nome>" << ansi::RESET << "\n";
        }
    }
    else if (command == "/scan" || command == "/host" || command == "/discover") {
        std::string target_host = arg.empty() ? "127.0.0.1" : arg;
        std::cout << ansi::DIM << "Escaneando host " << ansi::BOLD_YELLOW << target_host << ansi::RESET
                  << ansi::DIM << " em busca de servidores e modelos de LLM..." << ansi::RESET << "\n";

        providers::HostScanner scanner(target_host);
        auto services = scanner.scan();
        print_scan_results(target_host, services);

        if (!services.empty()) {
            for (const auto& s : services) {
                if (s.provider_type == "ollama") cfg.local_endpoints.ollama = s.base_url;
                if (s.provider_type == "llamacpp") cfg.local_endpoints.llamacpp = s.base_url;
            }

            if (command == "/host" || (services.size() == 1 && !services[0].models.empty())) {
                auto chosen_s = services[0];
                std::string m_name = chosen_s.models.empty() ? "default" : chosen_s.models[0];
                std::string full_m = chosen_s.provider_type + "/" + m_name;
                agent_->set_model(full_m);
                std::cout << ansi::BOLD_GREEN << "✓ Conectado com sucesso! Modelo ativo alterado para: " << ansi::BOLD_YELLOW << full_m << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/add") {
        if (arg.empty()) {
            std::cout << ansi::YELLOW << "Especifique o arquivo ou diretório: /add <caminho>" << ansi::RESET << "\n";
        } else {
            auto [ok, msg] = agent_->session().file_tracker().add_file(arg);
            std::string style = ok ? ansi::BOLD_GREEN : ansi::BOLD_RED;
            std::cout << style << msg << ansi::RESET << "\n";
        }
    }
    else if (command == "/drop") {
        if (arg.empty()) {
            std::cout << ansi::YELLOW << "Especifique o arquivo a remover: /drop <caminho>" << ansi::RESET << "\n";
        } else {
            if (agent_->session().file_tracker().remove_file(arg)) {
                std::cout << ansi::BOLD_GREEN << "Arquivo '" << arg << "' removido do contexto." << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_RED << "Arquivo '" << arg << "' não constava no contexto." << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/files") {
        auto files = agent_->session().file_tracker().list_files();
        if (files.empty()) {
            std::cout << ansi::DIM << "Nenhum arquivo adicionado ao contexto no momento. Use /add <arquivo> para incluir." << ansi::RESET << "\n";
        } else {
            std::cout << ansi::BOLD_CYAN << "Arquivos no Contexto (" << files.size() << "):" << ansi::RESET << "\n";
            for (const auto& f_name : files) {
                std::cout << "  📄 " << ansi::YELLOW << f_name << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/diff") {
        std::string diff_text = tools::get_git_diff();
        print_diff(diff_text, "Modificações Git Pendentes");
    }
    else if (command == "/commit") {
        if (arg.empty()) {
            std::string raw_diff = tools::get_raw_git_diff();
            if (raw_diff.empty()) {
                std::cout << ansi::YELLOW << i18n::t("commit_no_diff") << ansi::RESET << "\n";
            } else {
                std::cout << ansi::DIM << "Analisando alterações Git e gerando mensagem de commit semântica..." << ansi::RESET << "\n";
                std::string prompt = "Gere uma mensagem de commit curta, concisa e semântica no padrão Conventional Commits (ex: feat: ..., fix: ..., refactor: ...) para as seguintes alterações Git. Responda APENAS com a mensagem de commit, sem explicações:\n\n```diff\n" + raw_diff.substr(0, 3000) + "\n```";
                std::string proposed_msg = agent_->run_prompt(prompt);
                // Strip quotes/markdown
                auto mf = proposed_msg.find_first_not_of(" \t\r\n`'\"");
                auto ml = proposed_msg.find_last_not_of(" \t\r\n`'\"");
                if (mf != std::string::npos) proposed_msg = proposed_msg.substr(mf, ml - mf + 1);

                std::cout << "\n" << ansi::BOLD_GREEN << i18n::t("commit_suggested") << ansi::RESET << " " << ansi::BOLD_YELLOW << proposed_msg << ansi::RESET << "\n";
                if (!cfg.yolo_mode) {
                    std::string choice = ask_user_confirmation(i18n::t("confirm_commit"));
                    if (choice != "yes" && choice != "yolo") {
                        std::cout << ansi::DIM << "Commit cancelado." << ansi::RESET << "\n";
                        return true;
                    }
                    if (choice == "yolo") cfg.yolo_mode = true;
                }
                auto [ok, msg] = tools::create_user_commit(proposed_msg);
                std::string style = ok ? ansi::BOLD_GREEN : ansi::BOLD_RED;
                std::cout << style << msg << ansi::RESET << "\n";
            }
        } else {
            auto [ok, msg] = tools::create_user_commit(arg);
            std::string style = ok ? ansi::BOLD_GREEN : ansi::BOLD_RED;
            std::cout << style << msg << ansi::RESET << "\n";
        }
    }
    else if (command == "/review") {
        std::string raw_diff = tools::get_raw_git_diff();
        if (raw_diff.empty()) {
            std::cout << ansi::YELLOW << i18n::t("review_no_diff") << ansi::RESET << "\n";
        } else {
            std::cout << ansi::DIM << i18n::t("review_running") << ansi::RESET << "\n";
            std::string prompt = "Faça um Code Review técnico detalhado e construtivo das alterações Git abaixo.\nAvalie:\n1. 🐛 Possíveis bugs, regressões ou edge cases não tratados\n2. 🔒 Segurança e integridade de dados\n3. ⚡ Otimizações de desempenho e boas práticas de código\n4. 💡 Sugestões de melhoria\n\n```diff\n" + raw_diff.substr(0, 5000) + "\n```";
            agent_->run_prompt(prompt);
        }
    }
    else if (command == "/undo") {
        auto [ok, msg] = tools::undo_last_checkpoint();
        std::string style = ok ? ansi::BOLD_GREEN : ansi::BOLD_RED;
        std::cout << style << msg << ansi::RESET << "\n";
    }
    else if (command == "/run") {
        if (arg.empty()) {
            std::cout << ansi::YELLOW << "Especifique o comando a executar: /run <comando>" << ansi::RESET << "\n";
        } else {
            json args = {{"command", arg}};
            auto res = agent_->tools()["run_command"]->execute(args);
            std::cout << res.output << std::endl;
        }
    }
    else if (command == "/test") {
        std::string test_cmd = arg.empty() ? "make test" : arg;
        std::cout << ansi::DIM << "Executando testes: " << ansi::BOLD_YELLOW << test_cmd << ansi::RESET << ansi::DIM << "..." << ansi::RESET << "\n";
        json args = {{"command", test_cmd}};
        auto res = agent_->tools()["run_command"]->execute(args);
        std::cout << res.output << std::endl;

        if (!res.success) {
            std::cout << "\n" << ansi::BOLD_RED << "✗ Falha detectada nos testes." << ansi::RESET << "\n";
            bool should_fix = cfg.yolo_mode;
            if (!cfg.yolo_mode) {
                std::string choice = ask_user_confirmation("Deseja que a IA analise o erro e tente corrigir o código?");
                if (choice == "yes" || choice == "yolo") {
                    if (choice == "yolo") cfg.yolo_mode = true;
                    should_fix = true;
                }
            }
            if (should_fix) {
                agent_->run_prompt("Os testes falharam ao executar `" + test_cmd + "` com a seguinte saída:\n```\n" + res.output + "\n```\nPor favor, analise o erro e corrija o código para fazer os testes passarem.");
            }
        }
    }
    else if (command == "/gentest" || command == "/gentests" || command == "/test-for") {
        if (arg.empty()) {
            std::cout << ansi::DIM << "Use '/gentest <caminho_do_arquivo>' para gerar testes unitários automáticos (ex: /gentest src/config.cpp)." << ansi::RESET << "\n";
        } else {
            std::filesystem::path target = cfg.project_root / arg;
            if (!std::filesystem::exists(target)) {
                std::cout << ansi::RED << "Arquivo '" << arg << "' não encontrado." << ansi::RESET << "\n";
            } else {
                std::string prompt = tools::get_test_prompt_for_file(target, cfg.project_root);
                std::cout << ansi::BOLD_CYAN << "Gerando testes unitários completos para " << ansi::YELLOW << arg << ansi::RESET << "...\n";
                agent_->run_prompt(prompt);
            }
        }
    }
    else if (command == "/clear") {
        agent_->session().clear_history();
        std::cout << ansi::BOLD_GREEN << "Histórico da conversa limpo com sucesso." << ansi::RESET << "\n";
    }
    else if (command == "/reset") {
        if (arg == "prefs" || arg == "preferences" || arg == "config") {
            prefs.reset();
            cfg.yolo_mode = false;
            cfg.temperature = 0.2;
            std::cout << ansi::BOLD_GREEN << i18n::t("prefs_reset") << ansi::RESET << "\n";
        } else if (arg == "all") {
            agent_->session().clear_history();
            agent_->session().file_tracker().clear();
            prefs.reset();
            cfg.yolo_mode = false;
            cfg.temperature = 0.2;
            std::cout << ansi::BOLD_GREEN << i18n::t("all_reset") << ansi::RESET << "\n";
        } else {
            agent_->session().clear_history();
            agent_->session().file_tracker().clear();
            std::cout << ansi::GREEN << i18n::t("session_reset") << ansi::RESET << " " << ansi::DIM << "(Use '/reset prefs' para redefinir preferências salvas do usuário)" << ansi::RESET << "\n";
        }
    }
    else if (command == "/compact") {
        if (agent_->session().messages().empty()) {
            std::cout << ansi::DIM << i18n::t("compact_empty") << ansi::RESET << "\n";
        } else {
            std::cout << ansi::DIM << "Compactando histórico da conversa com resumo consolidado..." << ansi::RESET << "\n";
            std::ostringstream hist;
            for (const auto& m : agent_->session().messages()) {
                hist << m.role << ": " << m.content.substr(0, 200) << "\n";
            }
            std::string compact_prompt = "Resuma de forma concisa o histórico da conversa a seguir, preservando todas as decisões técnicas importantes, arquivos modificados e requisitos acordados:\n\n" + hist.str().substr(0, 4000);
            std::string summary = agent_->run_prompt(compact_prompt);
            agent_->session().compact_history(summary);
            int tokens = agent_->session().estimate_tokens();
            std::cout << ansi::BOLD_GREEN << i18n::t("compact_success", {{"tokens", std::to_string(tokens)}}) << ansi::RESET << "\n";
        }
    }
    else if (command == "/temp" || command == "/temperature") {
        if (arg.empty()) {
            std::cout << i18n::t("temp_current", {{"model", cfg.active_model}, {"temp", std::to_string(cfg.temperature)}}) << "\n";
            std::cout << ansi::DIM << "Use '/temp <valor>' (ex: /temp 0.0 para determinístico, /temp 0.7 para criativo)" << ansi::RESET << "\n";
        } else {
            try {
                double val = std::stod(arg);
                if (val < 0.0 || val > 2.0) {
                    std::cout << ansi::RED << i18n::t("temp_invalid") << ansi::RESET << "\n";
                } else {
                    cfg.temperature = val;
                    prefs.set_model_pref(cfg.active_model, "temperature", val);
                    prefs.set_global_pref("temperature", val);
                    std::cout << ansi::BOLD_GREEN << i18n::t("temp_changed", {{"model", cfg.active_model}, {"temp", std::to_string(val)}}) << ansi::RESET << "\n";
                }
            } catch (...) {
                std::cout << ansi::RED << i18n::t("temp_invalid") << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/system") {
        if (arg.empty()) {
            std::string cur;
            if (agent_->session().custom_system_prompt().has_value()) {
                cur = agent_->session().custom_system_prompt().value();
            } else {
                cur = agent_->session().build_system_message().content;
            }
            std::cout << "\n" << ansi::BOLD_CYAN << "=== SYSTEM PROMPT ATIVO ===" << ansi::RESET << "\n\n";
            std::cout << ansi::format_markdown(cur) << "\n\n";
            std::cout << ansi::DIM << "Dica: Use '/system <texto>' para alterar, '/system reset' para voltar ao padrão ou '/agents' para ver o AGENTS.md." << ansi::RESET << "\n";
        } else if (arg == "reset") {
            agent_->session().reset_system_prompt();
            std::cout << ansi::GREEN << i18n::t("sys_reset") << ansi::RESET << "\n";
        } else if (arg == "agents" || arg == "rules") {
            auto agents_rules = agent_->session().get_project_agents_rules();
            auto agents_path = agent_->session().get_project_agents_path();
            if (agents_rules.has_value() && !agents_rules.value().empty()) {
                std::cout << "\n" << ansi::BOLD_CYAN << "📜 Diretrizes do Projeto (" << agents_path.filename().string() << "):" << ansi::RESET << "\n";
                std::cout << ansi::DIM << "Caminho: " << agents_path.string() << ansi::RESET << "\n\n";
                std::cout << ansi::format_markdown(agents_rules.value()) << "\n";
            } else {
                std::cout << ansi::YELLOW << "Nenhum arquivo AGENTS.md, CLAUDE.md ou RULES.md encontrado na raiz do projeto ("
                          << get_config().project_root.string() << ")." << ansi::RESET << "\n";
            }
        } else {
            agent_->session().set_custom_system_prompt(arg);
            std::cout << ansi::BOLD_GREEN << i18n::t("sys_custom") << ansi::RESET << "\n";
        }
    }
    else if (command == "/agents" || command == "/rules") {
        auto agents_rules = agent_->session().get_project_agents_rules();
        auto agents_path = agent_->session().get_project_agents_path();
        if (agents_rules.has_value() && !agents_rules.value().empty()) {
            std::cout << "\n" << ansi::BOLD_CYAN << "📜 Diretrizes do Projeto (" << agents_path.filename().string() << "):" << ansi::RESET << "\n";
            std::cout << ansi::DIM << "Caminho: " << agents_path.string() << ansi::RESET << "\n\n";
            std::cout << ansi::format_markdown(agents_rules.value()) << "\n";
        } else {
            std::cout << ansi::YELLOW << "Nenhum arquivo AGENTS.md, CLAUDE.md ou RULES.md encontrado na raiz do projeto ("
                      << get_config().project_root.string() << ")." << ansi::RESET << "\n";
            std::cout << ansi::DIM << "Crie um arquivo AGENTS.md na pasta do seu projeto para definir regras personalizadas que serão seguidas pela LLM." << ansi::RESET << "\n";
        }
    }
    else if (command == "/paste") {
        std::cout << ansi::BOLD_CYAN << i18n::t("paste_active") << ansi::RESET << "\n";
        std::vector<std::string> lines;
        while (true) {
            std::cout << "... " << std::flush;
            std::string l_input;
            if (!std::getline(std::cin, l_input)) break;
            if (l_input == ":done") break;
            if (l_input == ":cancel") {
                std::cout << ansi::DIM << i18n::t("paste_cancel") << ansi::RESET << "\n";
                return true;
            }
            lines.push_back(l_input);
        }
        std::ostringstream full;
        for (const auto& l_str : lines) full << l_str << "\n";
        if (!full.str().empty()) {
            agent_->run_prompt(full.str());
        }
    }
    else if (command == "/index") {
        std::cout << ansi::DIM << "Indexando base de código para busca semântica..." << ansi::RESET << "\n";
        int count = agent_->indexer().index_codebase();
        std::cout << ansi::BOLD_GREEN << "✓ Base de código indexada com sucesso: " << ansi::BOLD_YELLOW << count << ansi::RESET << " blocos de código mapeados.\n";
    }
    else if (command == "/search") {
        if (arg.empty()) {
            std::cout << ansi::DIM << "Use '/search <termo/conceito/função>' para buscar trechos no código indexado." << ansi::RESET << "\n";
        } else {
            auto results = agent_->indexer().search(arg, 5);
            if (results.empty()) {
                std::cout << ansi::YELLOW << "Nenhum resultado relevante encontrado para: '" << arg << "'" << ansi::RESET << "\n";
            } else {
                std::cout << ansi::BOLD_CYAN << "Resultados da busca semântica para '" << arg << "':" << ansi::RESET << "\n";
                for (const auto& r : results) {
                    std::string header = r.chunk.file_path + " | " + r.chunk.symbol_type + ": " + r.chunk.symbol_name + " (Score: " + std::to_string(r.score) + ")";
                    ansi::print_panel(r.chunk.content, header, ansi::CYAN);
                }
            }
        }
    }
    else if (command == "/web") {
        if (arg.empty()) {
            std::cout << ansi::DIM << "Use '/web <pesquisa>' para pesquisar na web via DuckDuckGo/Tavily." << ansi::RESET << "\n";
        } else {
            json args = {{"query", arg}};
            auto res = agent_->tools()["web_search"]->execute(args);
            std::cout << res.output << std::endl;
        }
    }
    else if (command == "/todo") {
        if (arg.empty()) {
            std::cout << todo_manager_.format_checklist() << std::endl;
        } else if (arg.rfind("add ", 0) == 0) {
            auto item = todo_manager_.add_item(arg.substr(4));
            std::cout << ansi::BOLD_GREEN << "✓ Tarefa #" << item.id << " adicionada: " << ansi::RESET << item.text << "\n";
        } else if (arg.rfind("check ", 0) == 0 || arg.rfind("done ", 0) == 0) {
            try {
                auto sp = arg.find(' ');
                int tid = std::stoi(arg.substr(sp + 1));
                if (todo_manager_.check_item(tid)) {
                    std::cout << ansi::BOLD_GREEN << "✓ Tarefa #" << tid << " marcada como concluída!" << ansi::RESET << "\n";
                } else {
                    std::cout << ansi::RED << "Tarefa #" << tid << " não encontrada." << ansi::RESET << "\n";
                }
            } catch (...) {
                std::cout << ansi::RED << "Uso: /todo check <id>" << ansi::RESET << "\n";
            }
        } else if (arg == "clear") {
            todo_manager_.clear();
            std::cout << ansi::GREEN << "✓ Lista de tarefas limpa com sucesso." << ansi::RESET << "\n";
        } else {
            std::cout << ansi::DIM << "Uso: /todo, /todo add <tarefa>, /todo check <id>, /todo clear" << ansi::RESET << "\n";
        }
    }
    else if (command == "/plan") {
        if (arg.empty()) {
            std::cout << ansi::DIM << "Use '/plan <objetivo>' para planejar uma feature e gerar tarefas automáticas no /todo." << ansi::RESET << "\n";
        } else {
            std::string plan_prompt = "Crie um plano técnico detalhado passo a passo com checklist `- [ ]` para implementar o seguinte objetivo no projeto:\n\n" + arg;
            std::string resp = agent_->run_prompt(plan_prompt);
            int added = todo_manager_.parse_plan(resp);
            if (added > 0) {
                std::cout << "\n" << ansi::BOLD_GREEN << "✓ " << added << " tarefas extraídas automaticamente para o checklist /todo!" << ansi::RESET << "\n";
            }
        }
    }
    else if (command == "/export") {
        core::SessionExporter exporter(agent_->session(), cfg.project_root);
        std::string fmt = "md";
        std::filesystem::path target_p;

        if (!arg.empty()) {
            if (arg == "html" || arg == "htm") {
                fmt = "html";
            } else if (arg.rfind(".html") != std::string::npos) {
                fmt = "html";
                target_p = arg;
            } else {
                target_p = arg;
            }
        }

        try {
            std::filesystem::path out_p;
            if (fmt == "html") {
                out_p = exporter.export_html(target_p);
            } else {
                out_p = exporter.export_markdown(target_p);
            }
            std::cout << ansi::BOLD_GREEN << "✓ Sessão exportada com sucesso em: " << ansi::BOLD_YELLOW << out_p.string() << ansi::RESET << "\n";
        } catch (const std::exception& e) {
            std::cout << ansi::BOLD_RED << "Falha ao exportar sessão: " << e.what() << ansi::RESET << "\n";
        }
    }
    else if (command == "/mcp") {
        const auto& servers = agent_->mcp_manager().servers();
        if (servers.empty()) {
            std::cout << ansi::DIM << "Nenhum servidor MCP configurado. Crie um arquivo mcp_servers.json ou ~/.llmcli_mcp.json." << ansi::RESET << "\n";
        } else {
            std::cout << ansi::BOLD_CYAN << "Servidores MCP Configurados (" << servers.size() << "):" << ansi::RESET << "\n";
            for (const auto& [s_name, s_cfg] : servers) {
                std::cout << "  🔌 " << ansi::BOLD_YELLOW << s_name << ansi::RESET << ": comando='" << s_cfg.command << "'\n";
            }
        }
    }
    else if (command == "/tokens") {
        int tokens = agent_->session().estimate_tokens();
        auto [p_tok, c_tok, tot_tok] = agent_->session().get_cumulative_tokens();
        std::cout << "Estimativa atual de contexto: " << ansi::BOLD_CYAN << "~" << tokens << " tokens" << ansi::RESET << "\n";
        std::cout << "Tokens acumulados nesta sessão: " << ansi::BOLD_YELLOW << "~" << tot_tok << " tokens" << ansi::RESET
                  << " (~" << p_tok << " prompt + ~" << c_tok << " completion)\n";
    }
    else if (command == "/help") {
        print_help();
    }
    else {
        std::cout << ansi::RED << i18n::t("unknown_cmd", {{"cmd", command}}) << ansi::RESET << "\n";
    }

    return true;
}

static void repl_sigint_handler(int) {
    utils::HttpClient::cancel_active_stream();
}

void ReplSession::print_help() const {
    std::cout << "\n" << ansi::BOLD_CYAN << i18n::t("help_title") << ansi::RESET << "\n\n"
              << "  " << ansi::BOLD_YELLOW << "/yolo" << ansi::RESET << "             - " << i18n::t("cmd_yolo") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/architect [mod]" << ansi::RESET << "  - " << i18n::t("cmd_architect") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/dryrun" << ansi::RESET << "           - " << i18n::t("cmd_dryrun") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/lang [código]" << ansi::RESET << "    - " << i18n::t("cmd_lang") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/scan <ip/host>" << ansi::RESET << "   - " << i18n::t("cmd_scan") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/host <ip/host>" << ansi::RESET << "   - " << i18n::t("cmd_host") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/model <nome>" << ansi::RESET << "     - " << i18n::t("cmd_model") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/models" << ansi::RESET << "           - " << i18n::t("cmd_models") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/key [var] [val]" << ansi::RESET << "   - " << i18n::t("cmd_key") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/mcp" << ansi::RESET << "              - " << i18n::t("cmd_mcp") << "\n\n"
              << "  " << ansi::BOLD_YELLOW << "/add <caminho>" << ansi::RESET << "    - " << i18n::t("cmd_add") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/drop <caminho>" << ansi::RESET << "   - " << i18n::t("cmd_drop") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/files" << ansi::RESET << "            - " << i18n::t("cmd_files") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/index" << ansi::RESET << "            - " << i18n::t("cmd_index") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/search <termo>" << ansi::RESET << "  - " << i18n::t("cmd_search") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/web <termo>" << ansi::RESET << "     - " << i18n::t("cmd_web") << "\n\n"
              << "  " << ansi::BOLD_YELLOW << "/diff" << ansi::RESET << "             - " << i18n::t("cmd_diff") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/commit [msg]" << ansi::RESET << "      - " << i18n::t("cmd_commit") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/review" << ansi::RESET << "           - " << i18n::t("cmd_review") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/undo" << ansi::RESET << "             - " << i18n::t("cmd_undo") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/test [args]" << ansi::RESET << "      - " << i18n::t("cmd_test") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/gentest <arq>" << ansi::RESET << "    - " << i18n::t("cmd_gentest") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/run <comando>" << ansi::RESET << "   - " << i18n::t("cmd_run") << "\n\n"
              << "  " << ansi::BOLD_YELLOW << "/plan <objetivo>" << ansi::RESET << " - " << i18n::t("cmd_plan") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/todo [add|check]" << ansi::RESET << " - " << i18n::t("cmd_todo") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/export [md|html]" << ansi::RESET << " - " << i18n::t("cmd_export") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/paste" << ansi::RESET << "            - " << i18n::t("cmd_paste") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/compact" << ansi::RESET << "          - " << i18n::t("cmd_compact") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/temp [valor]" << ansi::RESET << "     - " << i18n::t("cmd_temp") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/system [txt]" << ansi::RESET << "     - " << i18n::t("cmd_system") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/agents" << ansi::RESET << "           - " << i18n::t("cmd_agents") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/clear" << ansi::RESET << "            - " << i18n::t("cmd_clear") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/reset [prefs|all]" << ansi::RESET << " - " << i18n::t("cmd_reset") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/repomap [on|off]" << ansi::RESET << " - " << i18n::t("cmd_repomap") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/session [cmd]" << ansi::RESET << "    - " << i18n::t("cmd_session") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/tokens" << ansi::RESET << "           - " << i18n::t("cmd_tokens") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/help" << ansi::RESET << "             - " << i18n::t("cmd_help") << "\n"
              << "  " << ansi::BOLD_YELLOW << "/exit, /quit" << ansi::RESET << "      - " << i18n::t("cmd_exit") << "\n\n";
}

void ReplSession::start() {
    Config& cfg = get_config();
    print_banner(cfg.active_model, cfg.yolo_mode);

    struct sigaction sa;
    sa.sa_handler = repl_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    while (true) {
        std::string prompt_str = get_prompt_str();
        std::string user_input = read_line_input(prompt_str);

        // Trim
        auto f = user_input.find_first_not_of(" \t\r\n");
        auto l = user_input.find_last_not_of(" \t\r\n");
        if (f == std::string::npos) continue;
        user_input = user_input.substr(f, l - f + 1);

        save_history_entry(user_input);

        if (user_input[0] == '/') {
            bool should_continue = handle_slash_command(user_input);
            if (!should_continue) break;
            continue;
        }

        // Process prompt with AI
        agent_->run_prompt(user_input);
    }
}

} // namespace llmcli::ui
