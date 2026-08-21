#include "llmcli/context/file_tracker.hpp"
#include "llmcli/config.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>
#include <fstream>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_file_tracker_suite() {
    run_test("Adicionar e remover arquivos do contexto", [](){
        llmcli::context::FileTracker tracker;
        // Create dummy file in project root
        auto& cfg = llmcli::get_config();
        std::filesystem::path dummy = cfg.project_root / "dummy_test.txt";
        {
            std::ofstream f(dummy);
            f << "Linha 1\nLinha 2\n";
        }

        auto [ok, msg] = tracker.add_file("dummy_test.txt");
        if (!ok) throw std::runtime_error("Falha ao adicionar arquivo: " + msg);
        if (tracker.size() != 1) throw std::runtime_error("Tamanho do tracker != 1");

        std::string ctx = tracker.get_context_text();
        if (ctx.find("Linha 1") == std::string::npos) throw std::runtime_error("Contexto não contém texto do arquivo");

        bool rem = tracker.remove_file("dummy_test.txt");
        if (!rem) throw std::runtime_error("Falha ao remover arquivo");
        if (tracker.size() != 0) throw std::runtime_error("Tamanho do tracker != 0 após remoção");

        std::filesystem::remove(dummy);
    });
}
