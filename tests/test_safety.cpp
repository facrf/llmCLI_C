#include "llmcli/core/exporter.hpp"
#include "llmcli/core/session.hpp"
#include "llmcli/tools/terminal.hpp"
#include <filesystem>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_safety_suite() {
    run_test("Terminal bloqueia sintaxe shell composta", []() {
        llmcli::tools::RunCommandTool tool;
        auto result = tool.execute({{"command", "echo seguro; echo inseguro"}});
        if (result.success || result.output.find("bloqueado") == std::string::npos) {
            throw std::runtime_error("Comando composto não foi bloqueado");
        }
    });

    run_test("Terminal bloqueia programas destrutivos", []() {
        llmcli::tools::RunCommandTool tool;
        auto result = tool.execute({{"command", "rm -rf /"}});
        if (result.success || result.output.find("bloqueado") == std::string::npos) {
            throw std::runtime_error("Programa destrutivo não foi bloqueado");
        }
    });

    run_test("Terminal executa comando simples no workspace", []() {
        llmcli::tools::RunCommandTool tool;
        auto result = tool.execute({{"command", "printf ok"}});
        if (!result.success || result.output.find("ok") == std::string::npos) {
            throw std::runtime_error("Comando simples não foi executado corretamente");
        }
    });

    run_test("Exportação rejeita destino externo", []() {
        llmcli::core::Session session;
        llmcli::core::SessionExporter exporter(session);
        bool rejected = false;
        try {
            exporter.export_markdown(std::filesystem::current_path().parent_path() / "outside_export.md");
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected) throw std::runtime_error("Exportação externa não foi rejeitada");
    });
}
