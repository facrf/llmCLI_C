#include "llmcli/ui/completer.hpp"
#include "llmcli/providers/registry.hpp"
#include "llmcli/i18n.hpp"
#include <algorithm>
#include <filesystem>

namespace llmcli::ui {

const std::vector<SlashCommandInfo>& get_all_slash_commands() {
    static std::string last_lang = "";
    static std::vector<SlashCommandInfo> commands;

    std::string cur_lang = i18n::get_active_language();
    if (commands.empty() || cur_lang != last_lang) {
        last_lang = cur_lang;
        commands = {
            {"/yolo", i18n::t("cmd_yolo")},
            {"/architect", i18n::t("cmd_architect")},
            {"/arch", i18n::t("cmd_arch")},
            {"/dryrun", i18n::t("cmd_dryrun")},
            {"/dry-run", i18n::t("cmd_dryrun")},
            {"/lang", i18n::t("cmd_lang")},
            {"/language", i18n::t("cmd_language")},
            {"/model", i18n::t("cmd_model")},
            {"/models", i18n::t("cmd_models")},
            {"/key", i18n::t("cmd_key")},
            {"/setkey", i18n::t("cmd_setkey")},
            {"/repomap", i18n::t("cmd_repomap")},
            {"/session", i18n::t("cmd_session")},
            {"/mcp", i18n::t("cmd_mcp")},
            {"/scan", i18n::t("cmd_scan")},
            {"/host", i18n::t("cmd_host")},
            {"/discover", i18n::t("cmd_discover")},
            {"/add", i18n::t("cmd_add")},
            {"/drop", i18n::t("cmd_drop")},
            {"/files", i18n::t("cmd_files")},
            {"/index", i18n::t("cmd_index")},
            {"/search", i18n::t("cmd_search")},
            {"/web", i18n::t("cmd_web")},
            {"/diff", i18n::t("cmd_diff")},
            {"/commit", i18n::t("cmd_commit")},
            {"/review", i18n::t("cmd_review")},
            {"/undo", i18n::t("cmd_undo")},
            {"/run", i18n::t("cmd_run")},
            {"/test", i18n::t("cmd_test")},
            {"/gentest", i18n::t("cmd_gentest")},
            {"/plan", i18n::t("cmd_plan")},
            {"/todo", i18n::t("cmd_todo")},
            {"/export", i18n::t("cmd_export")},
            {"/clear", i18n::t("cmd_clear")},
            {"/reset", i18n::t("cmd_reset")},
            {"/compact", i18n::t("cmd_compact")},
            {"/temp", i18n::t("cmd_temp")},
            {"/system", i18n::t("cmd_system")},
            {"/agents", i18n::t("cmd_agents")},
            {"/rules", i18n::t("cmd_rules")},
            {"/paste", i18n::t("cmd_paste")},
            {"/tokens", i18n::t("cmd_tokens")},
            {"/help", i18n::t("cmd_help")},
            {"/exit", i18n::t("cmd_exit")},
            {"/quit", i18n::t("cmd_quit")},
            {"/q", i18n::t("cmd_q")}
        };
    }
    return commands;
}

std::string compute_longest_common_prefix(const std::vector<std::string>& candidates) {
    if (candidates.empty()) return "";
    std::string common = candidates[0];
    for (size_t i = 1; i < candidates.size(); ++i) {
        size_t j = 0;
        while (j < common.size() && j < candidates[i].size() && common[j] == candidates[i][j]) {
            j++;
        }
        common = common.substr(0, j);
        if (common.empty()) break;
    }
    return common;
}

std::pair<std::optional<std::string>, std::vector<std::string>> resolve_slash_command(
    const std::string& command,
    bool has_arg
) {
    std::string clean = command;
    auto f = clean.find_first_not_of(" \t\r\n");
    auto l = clean.find_last_not_of(" \t\r\n");
    if (f == std::string::npos) return {std::nullopt, {}};
    clean = clean.substr(f, l - f + 1);
    std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char c){ return std::tolower(c); });

    if (clean.empty() || clean[0] != '/') return {std::nullopt, {}};

    const auto& all_cmds = get_all_slash_commands();

    // 1. Exact match
    for (const auto& item : all_cmds) {
        if (item.command == clean) {
            return {item.command, {}};
        }
    }

    // 2. Prefix search
    std::vector<std::string> matches;
    for (const auto& item : all_cmds) {
        if (item.command.rfind(clean, 0) == 0) {
            matches.push_back(item.command);
        }
    }

    if (matches.size() == 1) {
        return {matches[0], {}};
    }

    if (has_arg) {
        bool has_model = false;
        for (const auto& m : matches) {
            if (m == "/model") has_model = true;
        }
        if (has_model && (clean == "/m" || clean == "/mod" || clean == "/mode")) {
            return {"/model", {}};
        }
    }

    return {std::nullopt, matches};
}

std::vector<std::string> complete_files(const std::string& prefix, const std::string& root_dir) {
    std::vector<std::string> dirs;
    std::vector<std::string> files;
    try {
        std::filesystem::path root(root_dir.empty() ? "." : root_dir);
        std::filesystem::path search_dir = root;
        std::string sub_prefix = prefix;

        auto last_slash = prefix.find_last_of('/');
        if (last_slash != std::string::npos) {
            search_dir = root / prefix.substr(0, last_slash);
            sub_prefix = prefix.substr(last_slash + 1);
        }

        std::string p_lower = sub_prefix;
        std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });

        if (std::filesystem::exists(search_dir) && std::filesystem::is_directory(search_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(search_dir)) {
                std::string fname = entry.path().filename().string();
                if (fname.empty() || fname[0] == '.' || fname == "__pycache__" || fname == "node_modules" || fname == ".venv" || fname == "build") {
                    continue;
                }
                std::string fname_lower = fname;
                std::transform(fname_lower.begin(), fname_lower.end(), fname_lower.begin(), [](unsigned char c){ return std::tolower(c); });

                if (p_lower.empty() || fname_lower.rfind(p_lower, 0) == 0) {
                    std::string rel = std::filesystem::relative(entry.path(), root).string();
                    if (entry.is_directory()) {
                        dirs.push_back(rel + "/");
                    } else {
                        files.push_back(rel);
                    }
                }
            }
        }
    } catch (...) {}

    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    std::vector<std::string> results;
    results.reserve(dirs.size() + files.size());
    results.insert(results.end(), dirs.begin(), dirs.end());
    results.insert(results.end(), files.begin(), files.end());
    return results;
}

std::vector<std::string> complete_models(const std::string& prefix) {
    std::vector<std::string> results;
    auto presets = providers::ProviderRegistry::get_model_presets();
    std::string p_lower = prefix;
    std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& p : presets) {
        std::string m_lower = p.name;
        std::transform(m_lower.begin(), m_lower.end(), m_lower.begin(), [](unsigned char c){ return std::tolower(c); });
        
        std::string short_name = m_lower;
        auto slash_p = m_lower.find('/');
        if (slash_p != std::string::npos) short_name = m_lower.substr(slash_p + 1);

        if (p_lower.empty() ||
            m_lower.rfind(p_lower, 0) == 0 ||
            short_name.rfind(p_lower, 0) == 0 ||
            std::to_string(p.id) == prefix ||
            m_lower.find(p_lower) != std::string::npos) {
            results.push_back(p.name);
        }
    }
    return results;
}

std::vector<std::string> complete_languages(const std::string& prefix) {
    std::vector<std::string> results;
    std::vector<std::string> langs = {
        "pt-BR", "en-US", "es-ES", "de-DE", "fr-FR", "zh-CN", "ru-RU", "hi-IN", "auto",
        "pt", "en", "es", "de", "fr", "zh", "ru", "hi"
    };
    std::string p_lower = prefix;
    std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& l : langs) {
        std::string l_lower = l;
        std::transform(l_lower.begin(), l_lower.end(), l_lower.begin(), [](unsigned char c){ return std::tolower(c); });
        if (p_lower.empty() || l_lower.rfind(p_lower, 0) == 0) {
            results.push_back(l);
        }
    }
    return results;
}

std::vector<std::string> complete_keys(const std::string& prefix) {
    std::vector<std::string> results;
    std::vector<std::string> known_keys = {
        "GEMINI_API_KEY",
        "ANTHROPIC_API_KEY",
        "OPENAI_API_KEY",
        "GROQ_API_KEY",
        "DEEPSEEK_API_KEY",
        "TAVILY_API_KEY",
        "LLAMACPP_BASE_URL",
        "OLLAMA_BASE_URL",
        "LMSTUDIO_BASE_URL",
        "VLLM_BASE_URL",
        "DEFAULT_MODEL",
        "ARCHITECT_MODEL",
        "YOLO_MODE",
        "LLMCLI_LANG"
    };
    std::string p_upper = prefix;
    std::transform(p_upper.begin(), p_upper.end(), p_upper.begin(), [](unsigned char c){ return std::toupper(c); });

    for (const auto& k : known_keys) {
        if (p_upper.empty() || k.rfind(p_upper, 0) == 0 || k.find(p_upper) != std::string::npos) {
            results.push_back(k);
        }
    }
    return results;
}

} // namespace llmcli::ui
