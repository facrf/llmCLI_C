#pragma once

#include "llmcli/tools/base.hpp"
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <memory>

namespace llmcli::tools {

struct McpServerConfig {
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    bool enabled{true};

    json to_json() const {
        return json{
            {"command", command},
            {"args", args},
            {"env", env},
            {"enabled", enabled}
        };
    }

    static McpServerConfig from_json(const json& j) {
        McpServerConfig cfg;
        cfg.command = j.value("command", "");
        cfg.args = j.value("args", std::vector<std::string>());
        cfg.env = j.value("env", std::map<std::string, std::string>());
        cfg.enabled = j.value("enabled", true);
        return cfg;
    }
};

class McpTool : public BaseTool {
public:
    McpTool(
        const std::string& server_name,
        const std::string& tool_name,
        const std::string& description,
        const json& parameters
    );

    std::string name() const override { return full_name_; }
    std::string description() const override { return full_desc_; }
    json parameters_schema() const override { return parameters_; }

    ToolResult execute(const json& arguments) override;

private:
    std::string server_name_;
    std::string tool_name_;
    std::string full_name_;
    std::string full_desc_;
    json parameters_;
};

class McpManager {
public:
    explicit McpManager(const std::filesystem::path& project_root = "");

    void load_config();
    bool save_config(const std::filesystem::path& target_path = "");

    void add_server(
        const std::string& name,
        const std::string& command,
        const std::vector<std::string>& args = {},
        const std::map<std::string, std::string>& env = {}
    );

    const std::map<std::string, McpServerConfig>& servers() const { return servers_; }
    const std::vector<std::shared_ptr<McpTool>>& tools() const { return tools_; }

private:
    std::filesystem::path project_root_;
    std::map<std::string, McpServerConfig> servers_;
    std::vector<std::shared_ptr<McpTool>> tools_;

    std::vector<std::filesystem::path> get_config_paths() const;
};

} // namespace llmcli::tools
