#include "llmcli/i18n.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_i18n_suite() {
    run_test("Troca de idioma para Inglês e Português", [](){
        llmcli::i18n::set_active_language("en-US");
        if (llmcli::i18n::get_active_language() != "en-US") throw std::runtime_error("Falha ao definir en-US");

        std::string t_en = llmcli::i18n::t("exit_msg");
        if (t_en.find("Exiting") == std::string::npos) throw std::runtime_error("Tradução em inglês incorreta: " + t_en);

        llmcli::i18n::set_active_language("pt-BR");
        if (llmcli::i18n::get_active_language() != "pt-BR") throw std::runtime_error("Falha ao definir pt-BR");

        std::string t_pt = llmcli::i18n::t("exit_msg");
        if (t_pt.find("Encerrando") == std::string::npos) throw std::runtime_error("Tradução em português incorreta: " + t_pt);
    });

    run_test("Interpolação de parâmetros no i18n", [](){
        llmcli::i18n::set_active_language("pt-BR");
        std::string res = llmcli::i18n::t("yolo_on", {{"model", "gemini/flash"}});
        if (res.find("gemini/flash") == std::string::npos) {
            throw std::runtime_error("Parâmetro não interpolado: " + res);
        }
    });

    run_test("Resolução de aliases de idioma", [](){
        std::string res1 = llmcli::i18n::set_active_language("pt");
        if (res1 != "pt-BR") throw std::runtime_error("Alias 'pt' não mapeou para 'pt-BR'");

        std::string res2 = llmcli::i18n::set_active_language("es");
        if (res2 != "es-ES") throw std::runtime_error("Alias 'es' não mapeou para 'es-ES'");

        std::string res3 = llmcli::i18n::set_active_language("de");
        if (res3 != "de-DE") throw std::runtime_error("Alias 'de' não mapeou para 'de-DE'");

        llmcli::i18n::set_active_language("pt-BR");
    });
}
