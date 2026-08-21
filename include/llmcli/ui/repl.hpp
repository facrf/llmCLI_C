#pragma once

#include "llmcli/core/agent.hpp"
#include "llmcli/core/todo_manager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace llmcli::ui {

class ReplSession {
public:
    explicit ReplSession(std::shared_ptr<core::Agent> agent);

    void start();
    bool handle_slash_command(const std::string& cmd_line);

private:
    std::shared_ptr<core::Agent> agent_;
    core::TodoManager todo_manager_;
    std::filesystem::path history_path_;
    std::vector<std::string> history_;

    std::string get_prompt_str() const;
    void print_help() const;
    std::string read_line_input(const std::string& prompt);

    void load_history();
    void save_history_entry(const std::string& entry);
};

} // namespace llmcli::ui
