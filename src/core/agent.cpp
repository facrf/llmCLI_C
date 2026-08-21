#include "llmcli/core/agent.hpp"
#include "llmcli/config.hpp"
#include "llmcli/core/diff_applier.hpp"
#include "llmcli/providers/registry.hpp"
#include "llmcli/tools/filesystem.hpp"
#include "llmcli/tools/git_ops.hpp"
#include "llmcli/tools/terminal.hpp"
#include "llmcli/tools/web_tools.hpp"
#include "llmcli/ui/console.hpp"
#include "llmcli/utils/ansi.hpp"
#include "llmcli/utils/http.hpp"
#include "llmcli/i18n.hpp"
#include <iostream>
#include <sstream>

namespace llmcli::core {

// Semantic Search Tool adapter
class SemanticSearchTool : public tools::BaseTool {
public:
    explicit SemanticSearchTool(std::shared_ptr<context::SemanticIndexer> indexer)
        : indexer_(indexer) {}

    std::string name() const override { return "semantic_search"; }
    std::string description() const override {
        return "Busca funções, classes e trechos relevantes na base de código por significado semântico e palavras-chave.";
    }
    json parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "Termo de busca, conceito técnico ou nome de função/classe a pesquisar"}}},
                {"top_k", {{"type", "integer"}, {"description", "Número máximo de resultados a retornar (padrão: 5)"}, {"default", 5}}}
            }},
            {"required", json::array({"query"})}
        };
    }

    ToolResult execute(const json& arguments) override {
        ToolResult res;
        res.name = name();
        std::string query = arguments.value("query", "");
        int top_k = arguments.value("top_k", 5);

        if (query.empty()) {
            res.success = false;
            res.output = "Argumento 'query' obrigatório.";
            return res;
        }

        auto results = indexer_->search(query, top_k);
        if (results.empty()) {
            res.success = true;
            res.output = "Nenhum trecho de código relevante encontrado para: '" + query + "'";
            return res;
        }

        std::ostringstream out;
        for (const auto& r : results) {
            out << "--- [" << r.chunk.file_path << " | Linhas " << r.chunk.start_line << "-" << r.chunk.end_line
                << " | " << r.chunk.symbol_type << ": " << r.chunk.symbol_name << " (Score: " << r.score << ")] ---\n"
                << r.chunk.content << "\n\n";
        }
        res.success = true;
        res.output = out.str();
        return res;
    }

private:
    std::shared_ptr<context::SemanticIndexer> indexer_;
};

Agent::Agent(
    std::shared_ptr<Session> session,
    std::shared_ptr<providers::LLMProvider> architect_provider
) : session_(session ? session : std::make_shared<Session>()),
    architect_provider_(architect_provider) {

    Config& cfg = get_config();
    provider_ = providers::ProviderRegistry::create_provider();
    indexer_ = std::make_shared<context::SemanticIndexer>(cfg.project_root);
    mcp_manager_ = std::make_shared<tools::McpManager>(cfg.project_root);

    // Register built-in tools
    tools_["read_file"] = std::make_shared<tools::ReadFileTool>();
    tools_["write_file"] = std::make_shared<tools::WriteFileTool>();
    tools_["list_dir"] = std::make_shared<tools::ListDirTool>();
    tools_["grep_search"] = std::make_shared<tools::GrepSearchTool>();
    tools_["find_files"] = std::make_shared<tools::FindFilesTool>();
    tools_["run_command"] = std::make_shared<tools::RunCommandTool>();
    tools_["semantic_search"] = std::make_shared<SemanticSearchTool>(indexer_);
    tools_["web_search"] = std::make_shared<tools::WebSearchTool>();
    tools_["read_url"] = std::make_shared<tools::ReadUrlTool>();

    // Register MCP dynamic tools
    for (const auto& mcp_tool : mcp_manager_->tools()) {
        tools_[mcp_tool->name()] = mcp_tool;
    }
}

void Agent::set_model(const std::string& model_name) {
    Config& cfg = get_config();
    cfg.active_model = model_name;
    provider_ = providers::ProviderRegistry::create_provider(model_name);
    get_preferences().apply_model_preferences(model_name, cfg);
}

void Agent::refresh_provider() {
    Config& cfg = get_config();
    provider_ = providers::ProviderRegistry::create_provider(cfg.active_model);
}

std::vector<ToolDefinition> Agent::get_tool_definitions() const {
    std::vector<ToolDefinition> defs;
    for (const auto& [name, tool] : tools_) {
        defs.push_back(tool->get_definition());
    }
    return defs;
}

ToolResult Agent::execute_tool_with_permission(
    const std::string& tool_name,
    const json& arguments,
    const std::string& tool_call_id
) {
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) {
        ToolResult res;
        res.tool_call_id = tool_call_id;
        res.name = tool_name;
        res.success = false;
        res.output = "Ferramenta desconhecida: " + tool_name;
        return res;
    }

    Config& cfg = get_config();
    std::string args_str = arguments.dump();

    if (cfg.dry_run && (tool_name == "write_file" || tool_name == "run_command")) {
        ToolResult res;
        res.tool_call_id = tool_call_id;
        res.name = tool_name;
        res.success = true;
        if (tool_name == "write_file") {
            std::string p = arguments.value("path", "");
            res.output = "[DRY-RUN] Simulação: arquivo '" + p + "' seria gravado (gravação em disco ignorada).";
        } else {
            std::string c = arguments.value("command", "");
            res.output = "[DRY-RUN] Simulação: comando '" + c + "' seria executado (execução no terminal ignorada).";
        }
        ui::print_tool_execution(tool_name, args_str, true);
        ui::print_tool_result(tool_name, true, res.output);
        return res;
    }

    if (!cfg.yolo_mode && (tool_name == "write_file" || tool_name == "run_command")) {
        std::string confirm_msg = "Deseja executar a ferramenta '" + tool_name + "' com os argumentos: " + args_str + "?";
        std::string choice = ui::ask_user_confirmation(confirm_msg);
        if (choice == "abort") {
            return {tool_call_id, tool_name, false, "Ação cancelada pelo usuário."};
        }
        if (choice == "no") {
            return {tool_call_id, tool_name, false, "Ação rejeitada pelo usuário."};
        }
        if (choice == "yolo") {
            cfg.yolo_mode = true;
            std::cout << ansi::BOLD_RED << "⚡ Modo YOLO ativado para esta sessão!" << ansi::RESET << "\n";
        }
    }

    ui::print_tool_execution(tool_name, args_str, cfg.yolo_mode);
    ToolResult result = it->second->execute(arguments);
    result.tool_call_id = tool_call_id;

    // Checkpoint commit if modified files
    if (tool_name == "write_file" && result.success) {
        std::string p = arguments.value("path", "");
        auto hash = tools::create_checkpoint_commit("write_file em " + p);
        if (hash.has_value()) {
            result.output += "\n[Git Checkpoint: " + hash.value() + "]";
        }
    }

    ui::print_tool_result(tool_name, result.success, result.output);
    return result;
}

void Agent::print_token_usage(
    const std::vector<ChatMessage>& messages,
    const std::string& response_text,
    std::optional<int> native_prompt_tokens,
    std::optional<int> native_completion_tokens
) {
    bool is_exact = native_prompt_tokens.has_value() && native_completion_tokens.has_value();

    int prompt_tokens = native_prompt_tokens.value_or(0);
    int completion_tokens = native_completion_tokens.value_or(0);

    if (prompt_tokens <= 0) {
        size_t prompt_chars = 0;
        for (const auto& m : messages) prompt_chars += m.content.size();
        prompt_tokens = std::max(1, static_cast<int>(prompt_chars / 4));
    }
    if (completion_tokens <= 0) {
        completion_tokens = std::max(1, static_cast<int>(response_text.size() / 4));
    }
    int total_tokens = prompt_tokens + completion_tokens;

    session_->record_tokens(prompt_tokens, completion_tokens);
    auto [p_tok, c_tok, cumulative_total] = session_->get_cumulative_tokens();

    std::string prefix_symbol = is_exact ? "⚡ Tokens [exato]: " : "⚡ Tokens: ~";
    std::cout << ansi::DIM << prefix_symbol << prompt_tokens << " prompt + "
              << (is_exact ? "" : "~") << completion_tokens
              << " completion = " << (is_exact ? "" : "~") << total_tokens
              << " total | Sessão acumulada: ~" << cumulative_total
              << ansi::RESET << "\n";
}

std::shared_ptr<providers::LLMProvider> Agent::get_architect_provider() {
    if (architect_provider_) return architect_provider_;
    Config& cfg = get_config();
    std::string arch_model = cfg.architect_model.empty() ? "gemini/gemini-2.5-pro" : cfg.architect_model;
    return providers::ProviderRegistry::create_provider(arch_model);
}

std::string Agent::run_prompt(const std::string& user_prompt, int max_iterations) {
    Config& cfg = get_config();
    if (cfg.architect_mode) {
        return run_architect_pipeline(user_prompt, max_iterations);
    }

    session_->add_user_message(user_prompt);

    int iteration = 0;
    std::string final_assistant_text;

    while (iteration < max_iterations) {
        iteration++;
        auto messages = session_->get_full_messages();
        auto tools_defs = get_tool_definitions();

        std::cout << "\n" << ansi::DIM << "Pensando com " << ansi::BOLD_CYAN << cfg.active_model << ansi::RESET << ansi::DIM << "..." << ansi::RESET << "\n";

        std::string stream_text;
        std::vector<ToolCall> collected_tool_calls;
        bool has_error = false;
        std::optional<int> native_prompt_tok;
        std::optional<int> native_comp_tok;

        bool stream_ok = provider_->chat_stream(
            messages,
            tools_defs,
            [&](const StreamChunk& chunk) {
                if (chunk.prompt_tokens.has_value()) native_prompt_tok = chunk.prompt_tokens;
                if (chunk.completion_tokens.has_value()) native_comp_tok = chunk.completion_tokens;

                if (!chunk.error.empty()) {
                    std::cout << "\n" << ansi::BOLD_RED << "Erro da LLM: " << ansi::RESET << chunk.error << "\n";
                    has_error = true;
                    return;
                }
                if (!chunk.delta_content.empty()) {
                    std::cout << chunk.delta_content << std::flush;
                    stream_text += chunk.delta_content;
                }
                if (!chunk.tool_calls.empty()) {
                    collected_tool_calls.insert(collected_tool_calls.end(), chunk.tool_calls.begin(), chunk.tool_calls.end());
                }
            },
            cfg.temperature,
            cfg.max_tokens
        );

        std::cout << std::endl;

        if (utils::HttpClient::is_stream_cancelled()) {
            std::cout << "\n" << ansi::BOLD_YELLOW << i18n::t("stream_interrupted") << ansi::RESET << "\n";
            utils::HttpClient::reset_stream_cancel_flag();
            break;
        }

        if (!stream_text.empty()) {
            print_token_usage(messages, stream_text, native_prompt_tok, native_comp_tok);
        }

        if (!stream_ok || has_error) {
            session_->pop_last_user_message();
            std::string backup = providers::ProviderRegistry::find_backup_model(cfg.active_model);
            if (!backup.empty()) {
                std::cout << ansi::DIM << "💡 Dica: Provedor alternativo disponível: " << ansi::BOLD_GREEN << backup << ansi::RESET
                          << ". Use " << ansi::BOLD << "/model " << backup << ansi::RESET << " para alternar.\n";
            }
            break;
        }

        final_assistant_text = stream_text;

        // 1. Extract SEARCH/REPLACE blocks
        auto search_replace_blocks = extract_search_replace_blocks(stream_text);
        for (const auto& block : search_replace_blocks) {
            if (cfg.dry_run) {
                std::cout << ansi::BOLD_CYAN << "[DRY-RUN] Simulação de SEARCH/REPLACE em '" << block.file_path << "':" << ansi::RESET << "\n";
                std::string diff_preview = generate_unified_diff(block.search_content, block.replace_content, block.file_path);
                if (!diff_preview.empty()) {
                    ui::print_diff(diff_preview, block.file_path);
                }
                continue;
            }

            if (!cfg.yolo_mode) {
                std::string choice = ui::ask_user_confirmation("Aplicar modificação no arquivo '" + block.file_path + "'?");
                if (choice == "abort") break;
                if (choice == "yolo") {
                    cfg.yolo_mode = true;
                    std::cout << ansi::BOLD_RED << "⚡ Modo YOLO ativado!" << ansi::RESET << "\n";
                }
                if (choice == "no") continue;
            }

            auto [ok, msg, diff] = apply_search_replace_block(block);
            if (!diff.empty()) {
                ui::print_diff(diff, block.file_path);
            }
            ui::print_tool_result("diff_applier", ok, msg);
            if (ok) {
                tools::create_checkpoint_commit("patch em " + block.file_path);
            }
        }

        // 2. Extract JSON tool calls from markdown if none from native
        if (collected_tool_calls.empty()) {
            auto parsed_json_calls = extract_json_tool_calls(stream_text);
            if (!parsed_json_calls.empty()) {
                collected_tool_calls.insert(collected_tool_calls.end(), parsed_json_calls.begin(), parsed_json_calls.end());
            }
        }

        // 3. Execute tool calls if any
        if (!collected_tool_calls.empty()) {
            session_->add_assistant_message(stream_text, collected_tool_calls);
            for (const auto& tc : collected_tool_calls) {
                ToolResult res = execute_tool_with_permission(tc.name, tc.arguments, tc.id);
                session_->add_tool_result(tc.id, tc.name, res.output);
            }
            continue;
        }

        session_->add_assistant_message(stream_text);
        break;
    }

    return final_assistant_text;
}

std::string Agent::run_architect_pipeline(const std::string& user_prompt, int max_iterations) {
    session_->add_user_message(user_prompt);
    Config& cfg = get_config();
    std::string arch_model = cfg.architect_model.empty() ? "gemini/gemini-2.5-pro" : cfg.architect_model;
    std::string editor_model = cfg.active_model;

    std::cout << "\n" << ansi::BOLD_MAGENTA << "🏛️ [Arquiteto: " << arch_model << "] " << ansi::RESET
              << ansi::DIM << "Planejando solução..." << ansi::RESET << "\n";

    auto arch_p = get_architect_provider();
    auto messages = session_->get_full_messages();
    auto tools_defs = get_tool_definitions();

    std::string arch_text;
    std::optional<int> arch_prompt_tok;
    std::optional<int> arch_comp_tok;

    bool arch_err = false;
    bool stream_ok = arch_p->chat_stream(
        messages,
        tools_defs,
        [&](const StreamChunk& chunk) {
            if (chunk.prompt_tokens.has_value()) arch_prompt_tok = chunk.prompt_tokens;
            if (chunk.completion_tokens.has_value()) arch_comp_tok = chunk.completion_tokens;
            if (!chunk.error.empty()) {
                std::cout << "\n" << ansi::BOLD_RED << "Erro do Arquiteto: " << ansi::RESET << chunk.error << "\n";
                arch_err = true;
                return;
            }
            if (!chunk.delta_content.empty()) {
                std::cout << chunk.delta_content << std::flush;
                arch_text += chunk.delta_content;
            }
        },
        cfg.temperature,
        cfg.max_tokens
    );

    std::cout << std::endl;

    if (!arch_text.empty()) {
        print_token_usage(messages, arch_text, arch_prompt_tok, arch_comp_tok);
    }

    if (!stream_ok || arch_err || arch_text.empty()) {
        session_->pop_last_user_message();
        return "";
    }

    session_->add_assistant_message(arch_text);

    if (!cfg.yolo_mode) {
        std::string choice = ui::ask_user_confirmation("Deseja que o Editor (" + editor_model + ") aplique o plano nos arquivos?");
        if (choice == "abort" || choice == "no") {
            std::cout << ansi::DIM << "Aplicação do plano cancelada pelo usuário." << ansi::RESET << "\n";
            return arch_text;
        }
        if (choice == "yolo") {
            cfg.yolo_mode = true;
            std::cout << ansi::BOLD_RED << "⚡ Modo YOLO ativado!" << ansi::RESET << "\n";
        }
    }

    std::cout << "\n" << ansi::BOLD_CYAN << "⚡ [Editor: " << editor_model << "] " << ansi::RESET
              << ansi::DIM << "Aplicando alterações nos arquivos..." << ansi::RESET << "\n";

    std::string editor_prompt = "Você é o Editor de Código de alta velocidade do llmCli. "
                                "O Arquiteto elaborou o plano a seguir. Implemente com precisão todas as modificações necessárias usando ferramentas ou blocos SEARCH/REPLACE:\n\n" + arch_text;

    session_->add_user_message(editor_prompt);
    return run_editor_loop(max_iterations);
}

std::string Agent::run_editor_loop(int max_iterations) {
    Config& cfg = get_config();
    int iteration = 0;
    std::string final_assistant_text;

    while (iteration < max_iterations) {
        iteration++;
        auto messages = session_->get_full_messages();
        auto tools_defs = get_tool_definitions();

        std::string stream_text;
        std::vector<ToolCall> collected_tool_calls;
        bool has_error = false;
        std::optional<int> ed_prompt_tok;
        std::optional<int> ed_comp_tok;

        provider_->chat_stream(
            messages,
            tools_defs,
            [&](const StreamChunk& chunk) {
                if (chunk.prompt_tokens.has_value()) ed_prompt_tok = chunk.prompt_tokens;
                if (chunk.completion_tokens.has_value()) ed_comp_tok = chunk.completion_tokens;

                if (!chunk.error.empty()) {
                    std::cout << "\n" << ansi::BOLD_RED << "Erro do Editor: " << ansi::RESET << chunk.error << "\n";
                    has_error = true;
                    return;
                }
                if (!chunk.delta_content.empty()) {
                    std::cout << chunk.delta_content << std::flush;
                    stream_text += chunk.delta_content;
                }
                if (!chunk.tool_calls.empty()) {
                    collected_tool_calls.insert(collected_tool_calls.end(), chunk.tool_calls.begin(), chunk.tool_calls.end());
                }
            },
            cfg.temperature,
            cfg.max_tokens
        );

        std::cout << std::endl;

        if (!stream_text.empty()) {
            print_token_usage(messages, stream_text, ed_prompt_tok, ed_comp_tok);
        }

        if (has_error) break;

        final_assistant_text = stream_text;

        auto search_replace_blocks = extract_search_replace_blocks(stream_text);
        for (const auto& block : search_replace_blocks) {
            if (!cfg.yolo_mode) {
                std::string choice = ui::ask_user_confirmation("Aplicar modificação no arquivo '" + block.file_path + "'?");
                if (choice == "abort") break;
                if (choice == "yolo") {
                    cfg.yolo_mode = true;
                    std::cout << ansi::BOLD_RED << "⚡ Modo YOLO ativado!" << ansi::RESET << "\n";
                }
                if (choice == "no") continue;
            }

            auto [ok, msg, diff] = apply_search_replace_block(block);
            if (!diff.empty()) {
                ui::print_diff(diff, block.file_path);
            }
            ui::print_tool_result("diff_applier", ok, msg);
            if (ok) {
                tools::create_checkpoint_commit("patch em " + block.file_path);
            }
        }

        if (collected_tool_calls.empty()) {
            auto parsed_json_calls = extract_json_tool_calls(stream_text);
            if (!parsed_json_calls.empty()) {
                collected_tool_calls.insert(collected_tool_calls.end(), parsed_json_calls.begin(), parsed_json_calls.end());
            }
        }

        if (!collected_tool_calls.empty()) {
            session_->add_assistant_message(stream_text, collected_tool_calls);
            for (const auto& tc : collected_tool_calls) {
                ToolResult res = execute_tool_with_permission(tc.name, tc.arguments, tc.id);
                session_->add_tool_result(tc.id, tc.name, res.output);
            }
            continue;
        }

        session_->add_assistant_message(stream_text);
        break;
    }

    return final_assistant_text;
}

} // namespace llmcli::core
