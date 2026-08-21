#include "llmcli/tools/test_generator.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>

namespace llmcli::tools {

std::string get_test_prompt_for_file(
    const std::filesystem::path& target_file,
    const std::filesystem::path& project_root
) {
    std::filesystem::path root = project_root.empty() ? get_config().project_root : project_root;
    std::string content;

    std::ifstream f(target_file);
    if (f.is_open()) {
        std::stringstream ss;
        ss << f.rdbuf();
        content = ss.str();
    } else {
        content = "// Erro ao ler arquivo: " + target_file.string();
    }

    std::string rel_path;
    try {
        rel_path = std::filesystem::relative(target_file, root).string();
    } catch (...) {
        rel_path = target_file.string();
    }

    std::string ext = target_file.extension().string();
    std::string test_file_name;
    std::string test_framework;

    if (ext == ".py") {
        test_file_name = "tests/test_" + target_file.stem().string() + ".py";
        test_framework = "pytest";
    } else if (ext == ".js" || ext == ".jsx" || ext == ".mjs") {
        test_file_name = "tests/" + target_file.stem().string() + ".test" + ext;
        test_framework = "Jest / Vitest / Mocha";
    } else if (ext == ".ts" || ext == ".tsx") {
        test_file_name = "tests/" + target_file.stem().string() + ".test" + ext;
        test_framework = "Vitest / Jest";
    } else if (ext == ".go") {
        test_file_name = target_file.parent_path() / (target_file.stem().string() + "_test.go");
        test_framework = "Go standard testing ('testing' package)";
    } else if (ext == ".rs") {
        test_file_name = "tests/test_" + target_file.stem().string() + ".rs";
        test_framework = "Rust cargo test (#[cfg(test)])";
    } else if (ext == ".php") {
        test_file_name = "tests/" + target_file.stem().string() + "Test.php";
        test_framework = "PHPUnit";
    } else if (ext == ".java") {
        test_file_name = "src/test/java/" + target_file.stem().string() + "Test.java";
        test_framework = "JUnit 5";
    } else if (ext == ".rb") {
        test_file_name = "spec/" + target_file.stem().string() + "_spec.rb";
        test_framework = "RSpec";
    } else if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" || ext == ".cc") {
        test_file_name = "tests/test_" + target_file.stem().string() + ".cpp";
        test_framework = "C++ Test Runner / assert / framework";
    } else {
        test_file_name = "tests/test_" + target_file.stem().string() + ext;
        test_framework = "suíte de testes apropriada";
    }

    std::ostringstream out;
    out << "Escreva uma suíte completa de testes unitários com " << test_framework << " para o arquivo `" << rel_path << "`.\n\n"
        << "Requisitos:\n"
        << "1. Crie ou atualize o arquivo `" << test_file_name << "` usando a ferramenta `write_file` ou blocos SEARCH/REPLACE.\n"
        << "2. Cubra todos os caminhos principais (happy path), casos limites (edge cases) e tratamento de erros/exceções.\n"
        << "3. Mantenha o código limpo, modular e de execução rápida e determinística.\n\n"
        << "Código do arquivo `" << rel_path << "`:\n"
        << "```\n" << content << "\n```\n";

    return out.str();
}

} // namespace llmcli::tools
