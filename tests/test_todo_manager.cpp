#include "llmcli/core/todo_manager.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_todo_manager_suite() {
    run_test("Gerenciamento de Tarefas e Parse de Plano", [](){
        llmcli::core::TodoManager tm;
        auto item1 = tm.add_item("Criar testes unitários");
        if (item1.id != 1 || item1.done) throw std::runtime_error("Item inicial inválido");

        bool ok_chk = tm.check_item(1);
        if (!ok_chk || !tm.items()[0].done) throw std::runtime_error("Check falhou");

        std::string plan =
            "Plano de Ação:\n"
            "- [ ] 1. Configurar banco de dados\n"
            "- [x] 2. Criar migrations\n"
            "- [ ] 3. Implementar rotas\n";

        int added = tm.parse_plan(plan);
        if (added != 3) throw std::runtime_error("Esperadas 3 tarefas adicionadas, obtido " + std::to_string(added));
        if (tm.size() != 4) throw std::runtime_error("Total de tarefas != 4");

        std::string checklist = tm.format_checklist();
        if (checklist.find("Configurar banco de dados") == std::string::npos) {
            throw std::runtime_error("Formatação do checklist incorreta");
        }
    });
}
