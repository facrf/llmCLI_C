#include "llmcli/core/session.hpp"
#include "llmcli/config.hpp"
#include "llmcli/context/repomap.hpp"
#include "llmcli/i18n.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>

namespace llmcli::core {

static const char* SYSTEM_PROMPT_TEMPLATE =
R"(Você é o assistente oficial de desenvolvimento do llmCli, operando diretamente no terminal do desenvolvedor.
Você ajuda a criar, refatorar, depurar, testar e manter código em qualquer linguagem ou tecnologia com máxima precisão, performance e autonomia.

=== REGRAS FUNDAMENTAIS E SEGURANÇA ===
1. ISOLAMENTO DE DIRETÓRIO: Você opera estritamente dentro da raiz do projeto: {project_root}
   Nunca tente acessar, ler, gravar ou executar comandos fora deste diretório.
2. PRESERVAÇÃO DE CÓDIGO: Ao modificar arquivos existentes, preserve a formatação, comentários relevantes e evite remover código sem motivo.
3. SEGURANÇA: Nunca exponha segredos ou credenciais. Sugira o uso de variáveis no .env.
4. ADAPTABILIDADE: Adapte-se naturalmente às linguagens, padrões de projeto e frameworks utilizados no repositório.

=== ESTRATÉGIAS DE EDIÇÃO DE ARQUIVOS ===
Você pode usar as ferramentas fornecidas (Function Calling) OU gerar blocos SEARCH/REPLACE no estilo Aider:

Exemplo de bloco SEARCH/REPLACE:
Arquivo: caminho/do/arquivo.ext
<<<<<<< SEARCH
código exato existente
=======
código modificado
>>>>>>>

Para criar um novo arquivo completo, você pode usar a ferramenta `write_file` ou fornecer o SEARCH vazio:
Arquivo: novo_arquivo.ext
<<<<<<< SEARCH
=======
conteúdo completo do novo arquivo
>>>>>>>

=== ESTRUTURA ATUAL DO PROJETO ===
{repo_map}

{files_context}
)";

Session::Session() : file_tracker_(std::make_shared<context::FileTracker>()) {}

Session::Session(std::shared_ptr<context::FileTracker> file_tracker)
    : file_tracker_(file_tracker ? file_tracker : std::make_shared<context::FileTracker>()) {}

void Session::add_user_message(const std::string& text) {
    ChatMessage m;
    m.role = "user";
    m.content = text;
    messages_.push_back(m);
}

bool Session::pop_last_user_message() {
    if (!messages_.empty() && messages_.back().role == "user") {
        messages_.pop_back();
        return true;
    }
    return false;
}

void Session::add_assistant_message(const std::string& text, const std::vector<ToolCall>& tool_calls) {
    ChatMessage m;
    m.role = "assistant";
    m.content = text;
    m.tool_calls = tool_calls;
    messages_.push_back(m);
}

void Session::add_tool_result(const std::string& tool_call_id, const std::string& name, const std::string& output) {
    ChatMessage m;
    m.role = "tool";
    m.tool_call_id = tool_call_id;
    m.name = name;
    m.content = output;
    messages_.push_back(m);
}

void Session::clear_history() {
    messages_.clear();
}

void Session::set_custom_system_prompt(const std::string& prompt) {
    custom_system_prompt_ = prompt;
}

void Session::reset_system_prompt() {
    custom_system_prompt_ = std::nullopt;
}

void Session::record_tokens(int prompt_tokens, int completion_tokens) {
    cumulative_prompt_tokens_ += std::max(0, prompt_tokens);
    cumulative_completion_tokens_ += std::max(0, completion_tokens);
}

std::tuple<int, int, int> Session::get_cumulative_tokens() const {
    int total = cumulative_prompt_tokens_ + cumulative_completion_tokens_;
    return {cumulative_prompt_tokens_, cumulative_completion_tokens_, total};
}

int Session::estimate_tokens() const {
    auto all_msgs = get_full_messages();
    size_t total_chars = 0;
    for (const auto& m : all_msgs) {
        total_chars += m.content.size();
    }
    return std::max(1, static_cast<int>(total_chars / 4));
}

std::filesystem::path Session::get_project_agents_path() const {
    Config& cfg = get_config();
    std::vector<std::string> candidate_names = {
        "AGENTS.md", "agents.md", ".llmcli/AGENTS.md", "CLAUDE.md", "claude.md", "PROJECT.md", "RULES.md"
    };
    for (const auto& name : candidate_names) {
        std::filesystem::path p = cfg.project_root / name;
        if (std::filesystem::exists(p) && std::filesystem::is_regular_file(p)) {
            return p;
        }
    }
    return "";
}

std::optional<std::string> Session::get_project_agents_rules() const {
    auto p = get_project_agents_path();
    if (p.empty()) return std::nullopt;
    try {
        std::ifstream f(p);
        if (f.is_open()) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    } catch (...) {}
    return std::nullopt;
}

ChatMessage Session::build_system_message() const {
    if (custom_system_prompt_.has_value()) {
        ChatMessage m;
        m.role = "system";
        m.content = custom_system_prompt_.value();
        return m;
    }

    Config& cfg = get_config();
    std::string repo_map;
    if (cfg.enable_repomap) {
        repo_map = context::build_repo_map(cfg.project_root, cfg.repomap_max_files > 0 ? cfg.repomap_max_files : 60);
    } else {
        repo_map = "(RepoMap desativado pelo usuário para economia de tokens)";
    }
    std::string files_context = file_tracker_->get_context_text();

    std::string prompt_text = SYSTEM_PROMPT_TEMPLATE;

    // Replace placeholders
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };

    replace_all(prompt_text, "{project_root}", cfg.project_root.string());
    replace_all(prompt_text, "{repo_map}", repo_map);
    replace_all(prompt_text, "{files_context}", files_context);

    // Auto-inject project rules from AGENTS.md / CLAUDE.md / RULES.md
    auto agents_rules = get_project_agents_rules();
    if (agents_rules.has_value() && !agents_rules.value().empty()) {
        auto p_path = get_project_agents_path();
        std::string fname = p_path.empty() ? "AGENTS.md" : p_path.filename().string();
        prompt_text += "\n=== DIRETRIZES E REGRAS DO PROJETO (" + fname + ") ===\n" + agents_rules.value() + "\n======================================================\n";
    }

    std::string lang_inst = i18n::t("prompt_ai_instruction");
    if (!lang_inst.empty() && lang_inst != "prompt_ai_instruction") {
        prompt_text += "\n=== IDIOMA E COMUNICAÇÃO ===\n" + lang_inst + "\n";
    }

    ChatMessage m;
    m.role = "system";
    m.content = prompt_text;
    return m;
}

std::vector<ChatMessage> Session::get_full_messages() const {
    std::vector<ChatMessage> full;
    full.push_back(build_system_message());
    full.insert(full.end(), messages_.begin(), messages_.end());
    return full;
}

void Session::compact_history(const std::string& summary_text, size_t keep_last_n) {
    if (messages_.empty()) return;

    std::vector<ChatMessage> recent;
    if (messages_.size() > keep_last_n) {
        recent.insert(recent.end(), messages_.end() - keep_last_n, messages_.end());
    }

    ChatMessage summary_msg;
    summary_msg.role = "system";
    summary_msg.content = "=== RESUMO DO CONTEXTO ANTERIOR ===\n" + summary_text + "\n====================================";

    messages_.clear();
    messages_.push_back(summary_msg);
    messages_.insert(messages_.end(), recent.begin(), recent.end());
}

bool Session::save_to_file(const std::filesystem::path& file_path) const {
    try {
        if (!get_config().is_path_safe(file_path)) return false;
        if (file_path.has_parent_path() && !std::filesystem::exists(file_path.parent_path())) {
            std::filesystem::create_directories(file_path.parent_path());
        }

        json j;
        json msgs_arr = json::array();
        for (const auto& m : messages_) {
            msgs_arr.push_back(m.to_json());
        }
        j["messages"] = msgs_arr;

        json files_arr = json::array();
        for (const auto& f : file_tracker_->list_files()) {
            files_arr.push_back(f);
        }
        j["files"] = files_arr;

        if (custom_system_prompt_.has_value()) {
            j["custom_system_prompt"] = custom_system_prompt_.value();
        }

        j["cumulative_prompt_tokens"] = cumulative_prompt_tokens_;
        j["cumulative_completion_tokens"] = cumulative_completion_tokens_;

        std::ofstream out(file_path);
        if (!out.is_open()) return false;
        out << j.dump(2) << std::endl;
        return true;
    } catch (...) {
        return false;
    }
}

bool Session::load_from_file(const std::filesystem::path& file_path) {
    try {
        if (!get_config().is_path_safe(file_path)) return false;
        if (!std::filesystem::exists(file_path)) return false;
        std::ifstream in(file_path);
        if (!in.is_open()) return false;

        json j = json::parse(in);

        messages_.clear();
        if (j.contains("messages") && j["messages"].is_array()) {
            for (const auto& mj : j["messages"]) {
                ChatMessage m;
                m.role = mj.value("role", "user");
                m.content = mj.value("content", "");
                if (mj.contains("tool_calls") && mj["tool_calls"].is_array()) {
                    for (const auto& tcj : mj["tool_calls"]) {
                        ToolCall tc;
                        tc.id = tcj.value("id", "");
                        if (tcj.contains("function") && tcj["function"].is_object()) {
                            tc.name = tcj["function"].value("name", "");
                            tc.arguments = tcj["function"].value("arguments", json::object());
                        } else {
                            tc.name = tcj.value("name", "");
                            tc.arguments = tcj.value("arguments", json::object());
                        }
                        m.tool_calls.push_back(tc);
                    }
                }
                if (mj.contains("tool_call_id")) m.tool_call_id = mj["tool_call_id"].get<std::string>();
                if (mj.contains("name")) m.name = mj["name"].get<std::string>();
                messages_.push_back(m);
            }
        }

        file_tracker_->clear();
        if (j.contains("files") && j["files"].is_array()) {
            for (const auto& fj : j["files"]) {
                if (fj.is_string()) {
                    file_tracker_->add_file(fj.get<std::string>());
                }
            }
        }

        if (j.contains("custom_system_prompt") && j["custom_system_prompt"].is_string()) {
            custom_system_prompt_ = j["custom_system_prompt"].get<std::string>();
        } else {
            custom_system_prompt_ = std::nullopt;
        }

        cumulative_prompt_tokens_ = j.value("cumulative_prompt_tokens", 0);
        cumulative_completion_tokens_ = j.value("cumulative_completion_tokens", 0);

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace llmcli::core
