#pragma once

#include "llmcli/types.hpp"
#include "llmcli/context/file_tracker.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <tuple>

namespace llmcli::core {

class Session {
public:
    Session();
    explicit Session(std::shared_ptr<context::FileTracker> file_tracker);

    void add_user_message(const std::string& text);
    bool pop_last_user_message();
    void add_assistant_message(const std::string& text, const std::vector<ToolCall>& tool_calls = {});
    void add_tool_result(const std::string& tool_call_id, const std::string& name, const std::string& output);

    void clear_history();
    void set_custom_system_prompt(const std::string& prompt);
    void reset_system_prompt();

    ChatMessage build_system_message() const;
    std::vector<ChatMessage> get_full_messages() const;

    std::filesystem::path get_project_agents_path() const;
    std::optional<std::string> get_project_agents_rules() const;

    void compact_history(const std::string& summary_text, size_t keep_last_n = 2);

    bool save_to_file(const std::filesystem::path& file_path) const;
    bool load_from_file(const std::filesystem::path& file_path);

    void record_tokens(int prompt_tokens, int completion_tokens);
    std::tuple<int, int, int> get_cumulative_tokens() const;
    int estimate_tokens() const;

    context::FileTracker& file_tracker() { return *file_tracker_; }
    const context::FileTracker& file_tracker() const { return *file_tracker_; }

    const std::vector<ChatMessage>& messages() const { return messages_; }
    const std::optional<std::string>& custom_system_prompt() const { return custom_system_prompt_; }

private:
    std::shared_ptr<context::FileTracker> file_tracker_;
    std::vector<ChatMessage> messages_;
    std::optional<std::string> custom_system_prompt_;

    int cumulative_prompt_tokens_{0};
    int cumulative_completion_tokens_{0};
};

} // namespace llmcli::core
