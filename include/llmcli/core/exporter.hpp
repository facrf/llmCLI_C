#pragma once

#include "llmcli/core/session.hpp"
#include <filesystem>

namespace llmcli::core {

class SessionExporter {
public:
    explicit SessionExporter(const Session& session, const std::filesystem::path& project_root = "");

    std::filesystem::path export_markdown(const std::filesystem::path& target_path = "") const;
    std::filesystem::path export_html(const std::filesystem::path& target_path = "") const;

private:
    const Session& session_;
    std::filesystem::path project_root_;

    static std::string escape_html(const std::string& text);
    static std::string current_timestamp();
};

} // namespace llmcli::core
