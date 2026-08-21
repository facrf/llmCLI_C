#include "llmcli/ui/completer.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>
#include <algorithm>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_completer_suite() {
    run_test("Autocomplete de Comandos Slash (resolve_slash_command)", [](){
        auto [exact, m1] = llmcli::ui::resolve_slash_command("/model");
        if (!exact.has_value() || exact.value() != "/model") throw std::runtime_error("Exato /model falhou");

        auto [prefix, m2] = llmcli::ui::resolve_slash_command("/arch");
        if (!prefix.has_value() || prefix.value() != "/arch") throw std::runtime_error("Prefixo /arch falhou");

        auto [ambig, m3] = llmcli::ui::resolve_slash_command("/m");
        if (ambig.has_value()) throw std::runtime_error("Comando ambíguo /m não deveria resolver sem argumento");
        if (m3.empty()) throw std::runtime_error("Matches para /m vazios");

        auto [ambig_arg, m4] = llmcli::ui::resolve_slash_command("/m", true);
        if (!ambig_arg.has_value() || ambig_arg.value() != "/model") {
            throw std::runtime_error("/m com argumento deveria resolver para /model");
        }

        auto [key_cmd, m5] = llmcli::ui::resolve_slash_command("/key");
        if (!key_cmd.has_value() || key_cmd.value() != "/key") throw std::runtime_error("Comando /key não encontrado");

        auto [repo_cmd, m6] = llmcli::ui::resolve_slash_command("/repomap");
        if (!repo_cmd.has_value() || repo_cmd.value() != "/repomap") throw std::runtime_error("Comando /repomap não encontrado");

        auto [sess_cmd, m7] = llmcli::ui::resolve_slash_command("/session");
        if (!sess_cmd.has_value() || sess_cmd.value() != "/session") throw std::runtime_error("Comando /session não encontrado");

        auto [agents_cmd, m8] = llmcli::ui::resolve_slash_command("/agents");
        if (!agents_cmd.has_value() || agents_cmd.value() != "/agents") throw std::runtime_error("Comando /agents não encontrado");

        auto [rules_cmd, m9] = llmcli::ui::resolve_slash_command("/rules");
        if (!rules_cmd.has_value() || rules_cmd.value() != "/rules") throw std::runtime_error("Comando /rules não encontrado");
    });

    run_test("Autocomplete de Modelos e Categorias (complete_models)", [](){
        auto all_m = llmcli::ui::complete_models("");
        if (all_m.empty()) throw std::runtime_error("complete_models vazio para prefixo vazio");

        auto gemini_m = llmcli::ui::complete_models("gemini");
        if (gemini_m.empty()) throw std::runtime_error("complete_models não encontrou modelos gemini");

        auto flash_m = llmcli::ui::complete_models("flash");
        if (flash_m.empty()) throw std::runtime_error("complete_models não encontrou modelos pelo termo 'flash'");

        auto id_m = llmcli::ui::complete_models("1");
        if (id_m.empty()) throw std::runtime_error("complete_models não encontrou modelo por ID '1'");

        auto ge_m = llmcli::ui::complete_models("ge");
        std::string common_ge = llmcli::ui::compute_longest_common_prefix(ge_m);
        if (common_ge.find("gemini/") == std::string::npos) {
            throw std::runtime_error("LCP para prefixo 'ge' deveria conter 'gemini/'");
        }
    });

    run_test("Autocomplete de Idiomas e Variáveis (complete_languages & complete_keys)", [](){
        auto langs_pt = llmcli::ui::complete_languages("pt");
        if (langs_pt.empty()) throw std::runtime_error("complete_languages pt falhou");

        auto keys_gem = llmcli::ui::complete_keys("GEMINI");
        if (keys_gem.empty() || keys_gem[0] != "GEMINI_API_KEY") {
            throw std::runtime_error("complete_keys GEMINI falhou");
        }

        auto keys_url = llmcli::ui::complete_keys("OLLAMA");
        if (keys_url.empty() || keys_url[0] != "OLLAMA_BASE_URL") {
            throw std::runtime_error("complete_keys OLLAMA falhou");
        }
    });

    run_test("Cálculo do Maior Prefixo Comum (compute_longest_common_prefix)", [](){
        std::vector<std::string> candidates = {"/model", "/models"};
        std::string lcp = llmcli::ui::compute_longest_common_prefix(candidates);
        if (lcp != "/model") throw std::runtime_error("LCP para /model e /models deveria ser /model");

        std::vector<std::string> none = {"abc", "xyz"};
        if (!llmcli::ui::compute_longest_common_prefix(none).empty()) {
            throw std::runtime_error("LCP sem prefixo comum deveria ser vazio");
        }
    });
}
