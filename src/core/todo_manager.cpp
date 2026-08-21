#include "llmcli/core/todo_manager.hpp"
#include "llmcli/utils/ansi.hpp"
#include <sstream>
#include <regex>
#include <algorithm>

namespace llmcli::core {

TodoManager::TodoManager() = default;

TodoItem TodoManager::add_item(const std::string& text) {
    std::string clean = text;
    auto f = clean.find_first_not_of(" \t\r\n");
    auto l = clean.find_last_not_of(" \t\r\n");
    if (f != std::string::npos && l != std::string::npos) {
        clean = clean.substr(f, l - f + 1);
    }

    TodoItem item;
    item.id = next_id_++;
    item.text = clean;
    item.done = false;
    items_.push_back(item);
    return item;
}

bool TodoManager::check_item(int item_id) {
    for (auto& item : items_) {
        if (item.id == item_id) {
            item.done = true;
            return true;
        }
    }
    return false;
}

bool TodoManager::uncheck_item(int item_id) {
    for (auto& item : items_) {
        if (item.id == item_id) {
            item.done = false;
            return true;
        }
    }
    return false;
}

std::optional<bool> TodoManager::toggle_item(int item_id) {
    for (auto& item : items_) {
        if (item.id == item_id) {
            item.done = !item.done;
            return item.done;
        }
    }
    return std::nullopt;
}

bool TodoManager::remove_item(int item_id) {
    size_t before = items_.size();
    items_.erase(
        std::remove_if(items_.begin(), items_.end(), [item_id](const TodoItem& i){ return i.id == item_id; }),
        items_.end()
    );
    return items_.size() < before;
}

void TodoManager::clear() {
    items_.clear();
    next_id_ = 1;
}

int TodoManager::parse_plan(const std::string& plan_text) {
    int count = 0;
    std::stringstream ss(plan_text);
    std::string line;

    std::regex box_regex(R"(^[-*]\s*\[([ xX])\]\s*(.+)$)");
    std::regex num_regex(R"(^\d+[\.\)]\s*(.+)$)");
    std::regex bullet_regex(R"(^[-*]\s+(.+)$)");

    while (std::getline(ss, line)) {
        // Trim
        auto f = line.find_first_not_of(" \t\r\n");
        auto l = line.find_last_not_of(" \t\r\n");
        if (f == std::string::npos || l == std::string::npos) continue;
        std::string trimmed = line.substr(f, l - f + 1);

        std::smatch m;
        if (std::regex_match(trimmed, m, box_regex)) {
            char check_char = m[1].str()[0];
            bool is_done = (check_char == 'x' || check_char == 'X');
            std::string text = m[2].str();
            TodoItem item = add_item(text);
            item.done = is_done;
            count++;
            continue;
        }

        if (std::regex_match(trimmed, m, num_regex)) {
            std::string text = m[1].str();
            add_item(text);
            count++;
            continue;
        }

        if (std::regex_match(trimmed, m, bullet_regex)) {
            std::string text = m[1].str();
            if (text.size() > 5 && text.rfind("---", 0) != 0) {
                add_item(text);
                count++;
            }
        }
    }

    return count;
}

std::string TodoManager::format_checklist() const {
    if (items_.empty()) {
        return "Nenhuma tarefa na lista. Use '/todo add <tarefa>' ou '/plan <objetivo>' para adicionar.";
    }

    int completed = 0;
    for (const auto& item : items_) {
        if (item.done) completed++;
    }

    std::ostringstream out;
    out << ansi::BOLD_CYAN << "📋 Checklist de Tarefas ("
        << "Progresso: " << completed << "/" << items_.size() << " concluídas):" << ansi::RESET << "\n";

    for (const auto& item : items_) {
        std::string box = item.done ? std::string(ansi::BOLD_GREEN) + "[✓]" + ansi::RESET
                                    : std::string(ansi::BOLD_YELLOW) + "[ ]" + ansi::RESET;
        std::string text_styled = item.done ? std::string(ansi::DIM) + item.text + ansi::RESET
                                            : std::string(ansi::BOLD_WHITE) + item.text + ansi::RESET;

        out << "  " << box << " " << ansi::CYAN << "#" << item.id << ansi::RESET << " " << text_styled << "\n";
    }

    return out.str();
}

} // namespace llmcli::core
