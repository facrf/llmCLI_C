#include "llmcli/core/diff_applier.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_diff_applier_suite() {
    run_test("Extração de bloco SEARCH/REPLACE", [](){
        std::string sample =
            "Aqui está o código modificado:\n\n"
            "File: src/test.cpp\n"
            "<<<<<<< SEARCH\n"
            "int a = 1;\n"
            "=======\n"
            "int a = 2;\n"
            ">>>>>>>\n";

        auto blocks = llmcli::core::extract_search_replace_blocks(sample);
        if (blocks.size() != 1) throw std::runtime_error("Esperado 1 bloco, obtido " + std::to_string(blocks.size()));
        if (blocks[0].file_path != "src/test.cpp") throw std::runtime_error("Caminho incorreto: " + blocks[0].file_path);
        if (blocks[0].search_content.find("int a = 1;") == std::string::npos) throw std::runtime_error("SEARCH incorreto");
        if (blocks[0].replace_content.find("int a = 2;") == std::string::npos) throw std::runtime_error("REPLACE incorreto");
    });

    run_test("Fuzzy Find and Replace", [](){
        std::string original = "void foo() {\n    int x = 10;\n    return;\n}\n";
        std::string search = "    int x = 10;";
        std::string replace = "    int x = 20;";

        auto [ok, res] = llmcli::core::fuzzy_find_and_replace(original, search, replace);
        if (!ok) throw std::runtime_error("Substituição falhou");
        if (res.find("int x = 20;") == std::string::npos) throw std::runtime_error("Resultado incorreto: " + res);
    });

    run_test("Extração de chamadas de ferramentas em JSON", [](){
        std::string sample = "Vou ler o arquivo:\n```json\n{\"name\": \"read_file\", \"arguments\": {\"path\": \"src/main.cpp\"}}\n```";
        auto calls = llmcli::core::extract_json_tool_calls(sample);
        if (calls.size() != 1) throw std::runtime_error("Esperada 1 chamada JSON, obtido " + std::to_string(calls.size()));
        if (calls[0].name != "read_file") throw std::runtime_error("Nome incorreto: " + calls[0].name);
    });
}
