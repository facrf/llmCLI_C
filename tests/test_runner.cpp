#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include "llmcli/utils/ansi.hpp"

// Test function declarations
void test_config_suite();
void test_diff_applier_suite();
void test_todo_manager_suite();
void test_file_tracker_suite();
void test_semantic_indexer_suite();
void test_i18n_suite();
void test_completer_suite();
void test_safety_suite();

int g_passed = 0;
int g_failed = 0;

void run_test(const std::string& name, const std::function<void()>& fn) {
    try {
        fn();
        std::cout << llmcli::ansi::GREEN << "  [PASS] " << llmcli::ansi::RESET << name << "\n";
        g_passed++;
    } catch (const std::exception& e) {
        std::cout << llmcli::ansi::RED << "  [FAIL] " << llmcli::ansi::RESET << name << ": " << e.what() << "\n";
        g_failed++;
    } catch (...) {
        std::cout << llmcli::ansi::RED << "  [FAIL] " << llmcli::ansi::RESET << name << " (unknown exception)\n";
        g_failed++;
    }
}

int main() {
    std::cout << llmcli::ansi::BOLD_CYAN << "\n============================================\n"
              << "       🧪 llmCli C++ Test Runner\n"
              << "============================================\n" << llmcli::ansi::RESET << "\n";

    std::cout << llmcli::ansi::BOLD_YELLOW << "--- Testando Config & Environment ---" << llmcli::ansi::RESET << "\n";
    test_config_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando i18n & Idiomas ---" << llmcli::ansi::RESET << "\n";
    test_i18n_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando Diff Applier & Search/Replace ---" << llmcli::ansi::RESET << "\n";
    test_diff_applier_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando File Tracker ---" << llmcli::ansi::RESET << "\n";
    test_file_tracker_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando Semantic Indexer (BM25) ---" << llmcli::ansi::RESET << "\n";
    test_semantic_indexer_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando Todo Manager ---" << llmcli::ansi::RESET << "\n";
    test_todo_manager_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando Completer & Autocomplete ---" << llmcli::ansi::RESET << "\n";
    test_completer_suite();

    std::cout << "\n" << llmcli::ansi::BOLD_YELLOW << "--- Testando Proteções de Segurança ---" << llmcli::ansi::RESET << "\n";
    test_safety_suite();

    std::cout << "\n============================================\n";
    std::cout << "Resultados: " << llmcli::ansi::BOLD_GREEN << g_passed << " passaram" << llmcli::ansi::RESET
              << ", " << (g_failed > 0 ? llmcli::ansi::BOLD_RED : llmcli::ansi::DIM) << g_failed << " falharam" << llmcli::ansi::RESET << "\n";
    std::cout << "============================================\n\n";

    return (g_failed == 0) ? 0 : 1;
}
