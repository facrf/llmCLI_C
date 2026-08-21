#include "llmcli/ui/completer.hpp"
#include "llmcli/providers/registry.hpp"
#include "llmcli/i18n.hpp"
#include <algorithm>
#include <filesystem>

namespace llmcli::ui {

static const std::vector<SlashCommandInfo> ALL_SLASH_COMMANDS = {
    {"/yolo", "Alterna o modo YOLO (execução autônoma total sem pedir confirmação)"},
    {"/architect", "Alterna Modo Arquiteto (modelo forte para planejar + modelo rápido para editar)"},
    {"/arch", "Atalho para o comando /architect"},
    {"/lang", "Exibe ou altera o idioma do sistema (pt, en, es, de, fr, zh, ru, hi, auto)"},
    {"/language", "Atalho para o comando /lang"},
    {"/model", "Troca o modelo de LLM ativo (ex: /model llamacpp/default, /model gemini/gemini-2.5-flash)"},
    {"/models", "Lista todos os provedores e modelos locais/nuvem disponíveis"},
    {"/key", "Exibe ou configura chaves de API e variáveis de ambiente (ex: /key GEMINI_API_KEY ...)"},
    {"/setkey", "Atalho para configurar chaves de API no .env"},
    {"/repomap", "Alterna ou ajusta a árvore de RepoMap no contexto (ex: /repomap on, /repomap off, /repomap 30)"},
    {"/session", "Salva, carrega ou lista sessões de conversa (ex: /session save bugfix, /session load bugfix, /session list)"},
    {"/mcp", "Lista servidores MCP configurados e ferramentas externas dinâmicas"},
    {"/scan", "Escaneia um IP/host e detecta automaticamente todos os modelos e servidores de LLM ativos (ex: /scan 192.168.0.11)"},
    {"/host", "Conecta e define o IP/host padrão para Ollama e llama.cpp (ex: /host 192.168.0.11)"},
    {"/discover", "Escaneia e autodetecta modelos e servidores de LLM ativos no host"},
    {"/add", "Adiciona arquivo(s) ao contexto ativo da IA (ex: /add src/main.cpp)"},
    {"/drop", "Remove arquivo(s) do contexto da IA"},
    {"/files", "Lista arquivos atualmente carregados no contexto"},
    {"/index", "Indexa a base de código do projeto para busca semântica (RAG local)"},
    {"/search", "Busca funções, classes e trechos no código indexado (ex: /search banco de dados)"},
    {"/web", "Pesquisa informações e documentações na web (ex: /web cpp sockets)"},
    {"/diff", "Exibe as alterações git atuais não commitadas"},
    {"/commit", "Gera mensagem semântica via IA e cria commit Git (ex: /commit ou /commit feat: novo modulo)"},
    {"/review", "Analisa e faz Code Review das modificações Git pendentes"},
    {"/undo", "Reverte o último checkpoint / modificação realizada"},
    {"/run", "Executa um comando de terminal diretamente"},
    {"/test", "Executa a suíte de testes com diagnóstico"},
    {"/gentest", "Gera suíte completa de testes unitários para o arquivo (ex: /gentest src/config.cpp)"},
    {"/plan", "Cria plano técnico detalhado e divide automaticamente em tarefas no /todo"},
    {"/todo", "Exibe e gerencia o checklist interativo de tarefas da sessão (ex: /todo, /todo add, /todo check)"},
    {"/export", "Exporta a conversa e relatório da sessão em Markdown (.md) ou HTML (.html)"},
    {"/clear", "Limpa o histórico da conversa atual"},
    {"/reset", "Limpa histórico e remove todos os arquivos do contexto"},
    {"/compact", "Compacta o histórico da conversa gerando um resumo consolidado"},
    {"/temp", "Exibe ou altera a temperatura do modelo (ex: /temp 0.2)"},
    {"/system", "Exibe, altera ou redefine o system prompt da sessão (ex: /system reset, /system agents)"},
    {"/agents", "Exibe as diretrizes e regras do projeto carregadas do AGENTS.md (ou CLAUDE.md / RULES.md)"},
    {"/rules", "Atalho para o comando /agents (exibe regras do projeto)"},
    {"/paste", "Inicia modo de entrada multilinha para colar blocos de código"},
    {"/tokens", "Exibe estimativa de tokens do contexto atual"},
    {"/help", "Exibe o menu de ajuda e documentação"},
    {"/exit", "Encerra o llmCli C++"},
    {"/quit", "Encerra o llmCli C++"},
    {"/q", "Encerra o llmCli C++"}
};

const std::vector<SlashCommandInfo>& get_all_slash_commands() {
    return ALL_SLASH_COMMANDS;
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

    // 1. Exact match
    for (const auto& item : ALL_SLASH_COMMANDS) {
        if (item.command == clean) {
            return {item.command, {}};
        }
    }

    // 2. Prefix search
    std::vector<std::string> matches;
    for (const auto& item : ALL_SLASH_COMMANDS) {
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
