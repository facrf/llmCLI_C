#pragma once

#include <string>
#include <set>
#include <vector>
#include <filesystem>
#include <utility>

namespace llmcli::context {

class FileTracker {
public:
    FileTracker() = default;

    std::pair<bool, std::string> add_file(const std::string& file_path);
    bool remove_file(const std::string& file_path);
    void clear();

    std::vector<std::string> list_files() const;
    std::string get_context_text() const;

    size_t size() const { return tracked_files_.size(); }
    const std::set<std::string>& tracked_files() const { return tracked_files_; }

private:
    std::set<std::string> tracked_files_;
};

} // namespace llmcli::context
