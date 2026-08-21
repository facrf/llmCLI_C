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

    run_test("Traduções de Dry-Run e Comandos em múltiplos idiomas", [](){
        llmcli::i18n::set_active_language("pt-BR");
        std::string pt_dry = llmcli::i18n::t("dryrun_on");
        if (pt_dry.find("DRY-RUN") == std::string::npos) throw std::runtime_error("dryrun_on em pt-BR incorreto");

        std::string pt_cmd = llmcli::i18n::t("cmd_yolo");
        if (pt_cmd.empty() || pt_cmd == "cmd_yolo") throw std::runtime_error("cmd_yolo em pt-BR não traduzido");

        llmcli::i18n::set_active_language("en-US");
        std::string en_dry = llmcli::i18n::t("dryrun_on");
        if (en_dry.find("DRY-RUN MODE ENABLED") == std::string::npos) throw std::runtime_error("dryrun_on em en-US incorreto");

        std::string en_cmd = llmcli::i18n::t("cmd_yolo");
        if (en_cmd.find("Toggles YOLO") == std::string::npos) throw std::runtime_error("cmd_yolo em en-US incorreto: " + en_cmd);

        llmcli::i18n::set_active_language("es-ES");
        std::string es_help = llmcli::i18n::t("help_title");
        if (es_help.find("Comandos") == std::string::npos) throw std::runtime_error("help_title em es-ES incorreto: " + es_help);

        llmcli::i18n::set_active_language("pt-BR");
    });
}
