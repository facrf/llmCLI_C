CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2 -pthread -Iinclude
BIN_DIR = bin
BUILD_DIR = build

# Source files
CORE_SRCS = \
	src/config.cpp \
	src/i18n.cpp \
	src/utils/ansi.cpp \
	src/utils/env.cpp \
	src/utils/http.cpp \
	src/context/file_tracker.cpp \
	src/context/repomap.cpp \
	src/context/semantic_indexer.cpp \
	src/core/agent.cpp \
	src/core/diff_applier.cpp \
	src/core/exporter.cpp \
	src/core/session.cpp \
	src/core/todo_manager.cpp \
	src/providers/anthropic.cpp \
	src/providers/gemini.cpp \
	src/providers/llamacpp.cpp \
	src/providers/ollama.cpp \
	src/providers/openai_compatible.cpp \
	src/providers/registry.cpp \
	src/providers/scanner.cpp \
	src/tools/filesystem.cpp \
	src/tools/git_ops.cpp \
	src/tools/mcp_client.cpp \
	src/tools/terminal.cpp \
	src/tools/test_generator.cpp \
	src/tools/web_tools.cpp \
	src/ui/completer.cpp \
	src/ui/console.cpp \
	src/ui/repl.cpp

MAIN_SRC = src/main.cpp

TEST_SRCS = \
	tests/test_runner.cpp \
	tests/test_config.cpp \
	tests/test_i18n.cpp \
	tests/test_diff_applier.cpp \
	tests/test_file_tracker.cpp \
	tests/test_semantic_indexer.cpp \
	tests/test_todo_manager.cpp \
	tests/test_completer.cpp

CORE_OBJS = $(CORE_SRCS:src/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJ = $(BUILD_DIR)/main.o
TEST_OBJS = $(TEST_SRCS:tests/%.cpp=$(BUILD_DIR)/tests/%.o)

TARGET = $(BIN_DIR)/llm-cli
TEST_TARGET = $(BIN_DIR)/test_runner

.PHONY: all clean test dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/utils
	@mkdir -p $(BUILD_DIR)/context
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/providers
	@mkdir -p $(BUILD_DIR)/tools
	@mkdir -p $(BUILD_DIR)/ui
	@mkdir -p $(BUILD_DIR)/tests

$(BUILD_DIR)/%.o: src/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: dirs $(TEST_TARGET)
	@./$(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
