#include "llmcli/config.hpp"
#include "llmcli/utils/env.hpp"
#include "llmcli/core/session.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_config_suite() {
    run_test("Carregamento de configuração padrão", [](){
        llmcli::Config cfg = llmcli::Config::load();
        if (cfg.default_model.empty()) throw std::runtime_error("default_model vazio");
        if (cfg.active_model.empty()) throw std::runtime_error("active_model vazio");
        if (cfg.temperature < 0.0 || cfg.temperature > 2.0) throw std::runtime_error("temperature fora da faixa");
    });

    run_test("Verificação de segurança de caminhos (is_path_safe)", [](){
        llmcli::Config cfg = llmcli::Config::load();
        auto inside = cfg.project_root / "src" / "main.cpp";
        if (!cfg.is_path_safe(inside)) throw std::runtime_error("Caminho interno marcado como inseguro");

        auto outside = cfg.project_root.parent_path().parent_path() / "etc" / "passwd";
        if (cfg.is_path_safe(outside)) throw std::runtime_error("Caminho externo marcado como seguro");
    });

    run_test("Persistência de Preferências do Usuário", [](){
        llmcli::UserPreferences prefs(std::filesystem::current_path() / "test_prefs.json");
        prefs.set_global_pref("test_key", "test_val");
        if (prefs.get_global_pref("test_key").get<std::string>() != "test_val") {
            throw std::runtime_error("Falha ao salvar/ler preferência global");
        }

        prefs.set_model_pref("llamacpp/default", "temperature", 0.7);
        if (prefs.get_model_pref("llamacpp/default", "temperature").get<double>() != 0.7) {
            throw std::runtime_error("Falha ao salvar/ler preferência por modelo");
        }

        prefs.reset();
        if (!prefs.get_global_pref("test_key").is_null()) {
            throw std::runtime_error("Reset não limpou preferências");
        }
        std::filesystem::remove("test_prefs.json");
    });

    run_test("Persistência e descoberta de variáveis .env (save_env_var)", [](){
        std::filesystem::path test_env = std::filesystem::current_path() / "test_scratch.env";
        bool ok = llmcli::utils::EnvLoader::save_env_var("TEST_CUSTOM_KEY", "secret123", test_env);
        if (!ok) throw std::runtime_error("save_env_var falhou ao criar arquivo");
        if (llmcli::utils::EnvLoader::get_env("TEST_CUSTOM_KEY") != "secret123") {
            throw std::runtime_error("EnvLoader não atualizou o ambiente em memória");
        }

        // Test update existing key
        ok = llmcli::utils::EnvLoader::save_env_var("TEST_CUSTOM_KEY", "updated456", test_env);
        if (!ok) throw std::runtime_error("save_env_var falhou ao atualizar");
        if (llmcli::utils::EnvLoader::get_env("TEST_CUSTOM_KEY") != "updated456") {
            throw std::runtime_error("EnvLoader não atualizou valor alterado");
        }
        std::filesystem::remove(test_env);
    });

    run_test("Rollback de mensagem em falha de sessão (pop_last_user_message)", [](){
        llmcli::core::Session sess;
        sess.add_user_message("prompt de teste");
        if (sess.messages().size() != 1) throw std::runtime_error("add_user_message falhou");
        bool popped = sess.pop_last_user_message();
        if (!popped || !sess.messages().empty()) {
            throw std::runtime_error("pop_last_user_message falhou ao remover última mensagem de usuário");
        }
        // Popping on empty should return false
        if (sess.pop_last_user_message()) {
            throw std::runtime_error("pop_last_user_message em sessão vazia deveria retornar false");
        }
    });

    run_test("Persistência de Sessão (save_to_file e load_from_file)", [](){
        std::filesystem::path sess_file = std::filesystem::current_path() / "test_session_dump.json";
        llmcli::core::Session sess1;
        sess1.add_user_message("Como criar um servidor em C++?");
        sess1.add_assistant_message("Aqui está um exemplo simples com sockets.");
        sess1.set_custom_system_prompt("System prompt personalizado de teste");
        sess1.record_tokens(150, 45);

        bool saved = sess1.save_to_file(sess_file);
        if (!saved) throw std::runtime_error("save_to_file falhou");

        llmcli::core::Session sess2;
        bool loaded = sess2.load_from_file(sess_file);
        if (!loaded) throw std::runtime_error("load_from_file falhou");

        if (sess2.messages().size() != 2) throw std::runtime_error("Mensagens não foram restauradas corretamente");
        if (sess2.messages()[0].content != "Como criar um servidor em C++?") throw std::runtime_error("Conteúdo da mensagem restaurada incorreto");
        if (!sess2.custom_system_prompt().has_value() || sess2.custom_system_prompt().value() != "System prompt personalizado de teste") {
            throw std::runtime_error("Custom system prompt não restaurado");
        }
        auto [p, c, tot] = sess2.get_cumulative_tokens();
        if (p != 150 || c != 45 || tot != 195) throw std::runtime_error("Tokens acumulados não restaurados corretamente");

        std::filesystem::remove(sess_file);
    });

    run_test("Controle dinâmico de RepoMap (enable_repomap)", [](){
        llmcli::Config& cfg = llmcli::get_config();
        bool old_enable = cfg.enable_repomap;

        cfg.enable_repomap = false;
        llmcli::core::Session sess_no_map;
        auto msg_no_map = sess_no_map.build_system_message();
        if (msg_no_map.content.find("RepoMap desativado") == std::string::npos) {
            throw std::runtime_error("build_system_message não indicou que repomap está desativado");
        }

        cfg.enable_repomap = true;
        llmcli::core::Session sess_with_map;
        auto msg_with_map = sess_with_map.build_system_message();
        if (msg_with_map.content.find("RepoMap desativado") != std::string::npos) {
            throw std::runtime_error("build_system_message não incluiu repomap quando ativado");
        }

        cfg.enable_repomap = old_enable;
    });

    run_test("Auto-descoberta e injeção de AGENTS.md no Contexto", [](){
        llmcli::core::Session sess;
        auto agents_rules = sess.get_project_agents_rules();
        if (!agents_rules.has_value() || agents_rules.value().empty()) {
            throw std::runtime_error("AGENTS.md deveria ter sido descoberto no diretório raiz do projeto");
        }
        auto sys_msg = sess.build_system_message();
        if (sys_msg.content.find("DIRETRIZES E REGRAS DO PROJETO") == std::string::npos) {
            throw std::runtime_error("build_system_message não injetou o bloco AGENTS.md");
        }
    });
}
