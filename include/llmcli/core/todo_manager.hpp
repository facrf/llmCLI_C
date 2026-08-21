#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <vector>
#include <optional>

namespace llmcli::core {

class TodoManager {
public:
    TodoManager();

    TodoItem add_item(const std::string& text);
    bool check_item(int item_id);
    bool uncheck_item(int item_id);
    std::optional<bool> toggle_item(int item_id);
    bool remove_item(int item_id);
    void clear();

    int parse_plan(const std::string& plan_text);
    std::string format_checklist() const;

    const std::vector<TodoItem>& items() const { return items_; }
    size_t size() const { return items_.size(); }

private:
    std::vector<TodoItem> items_;
    int next_id_{1};
};

} // namespace llmcli::core
