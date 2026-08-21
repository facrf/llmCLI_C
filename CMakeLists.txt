cmake_minimum_required(VERSION 3.16)
project(llmCli_C VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Core library sources
set(CORE_SOURCES
    src/config.cpp
    src/i18n.cpp
    src/utils/ansi.cpp
    src/utils/env.cpp
    src/utils/http.cpp
    src/context/file_tracker.cpp
    src/context/repomap.cpp
    src/context/semantic_indexer.cpp
    src/core/agent.cpp
    src/core/diff_applier.cpp
    src/core/exporter.cpp
    src/core/session.cpp
    src/core/todo_manager.cpp
    src/providers/anthropic.cpp
    src/providers/gemini.cpp
    src/providers/llamacpp.cpp
    src/providers/ollama.cpp
    src/providers/openai_compatible.cpp
    src/providers/registry.cpp
    src/providers/scanner.cpp
    src/tools/filesystem.cpp
    src/tools/git_ops.cpp
    src/tools/mcp_client.cpp
    src/tools/terminal.cpp
    src/tools/test_generator.cpp
    src/tools/web_tools.cpp
    src/ui/completer.cpp
    src/ui/console.cpp
    src/ui/repl.cpp
)

add_library(llmcli_core STATIC ${CORE_SOURCES})
target_include_directories(llmcli_core PUBLIC include)

# Main Executable
add_executable(llm-cli src/main.cpp)
target_link_libraries(llm-cli PRIVATE llmcli_core pthread)
set_target_properties(llm-cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Test Executable
add_executable(test_runner
    tests/test_runner.cpp
    tests/test_config.cpp
    tests/test_i18n.cpp
    tests/test_diff_applier.cpp
    tests/test_file_tracker.cpp
    tests/test_semantic_indexer.cpp
    tests/test_todo_manager.cpp
)
target_link_libraries(test_runner PRIVATE llmcli_core pthread)
set_target_properties(test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
