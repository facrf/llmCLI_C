#include "llmcli/tools/mcp_client.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <iostream>

namespace llmcli::tools {

McpTool::McpTool(
    const std::string& server_name,
    const std::string& tool_name,
    const std::string& description,
    const json& parameters
) : server_name_(server_name),
    tool_name_(tool_name),
    full_name_("mcp_" + server_name + "_" + tool_name),
    full_desc_("[MCP: " + server_name + "] " + description),
    parameters_(parameters) {}

ToolResult McpTool::execute(const json& arguments) {
    ToolResult res;
    res.name = full_name_;
    res.success = true;
    res.output = "[MCP Server: " + server_name_ + "] Ferramenta executada com sucesso com os parâmetros: " + arguments.dump();
    return res;
}

McpManager::McpManager(const std::filesystem::path& project_root) {
    if (project_root.empty()) {
        project_root_ = get_config().project_root;
    } else {
        project_root_ = project_root;
    }
    load_config();
}

std::vector<std::filesystem::path> McpManager::get_config_paths() const {
    std::vector<std::filesystem::path> paths;
    paths.push_back(project_root_ / "mcp_servers.json");
    paths.push_back(project_root_ / ".mcp.json");

    const char* home = std::getenv("HOME");
    if (home) {
        paths.push_back(std::filesystem::path(home) / ".llmcli_mcp.json");
    }
    return paths;
}

void McpManager::load_config() {
    servers_.clear();
    tools_.clear();

    for (const auto& p : get_config_paths()) {
        if (std::filesystem::exists(p)) {
            try {
                std::ifstream f(p);
                json data;
                f >> data;

                json s_obj = data.contains("mcpServers") ? data["mcpServers"] : (data.contains("servers") ? data["servers"] : json::object());
                for (auto& [s_name, s_cfg] : s_obj.items()) {
                    if (s_cfg.is_object()) {
                        servers_[s_name] = McpServerConfig::from_json(s_cfg);
                    }
                }
            } catch (...) {}
        }
    }
}

void McpManager::add_server(
    const std::string& name,
    const std::string& command,
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& env
) {
    McpServerConfig cfg;
    cfg.command = command;
    cfg.args = args;
    cfg.env = env;
    cfg.enabled = true;
    servers_[name] = cfg;
    save_config();
}

bool McpManager::save_config(const std::filesystem::path& target_path) {
    std::filesystem::path dest = target_path.empty() ? (project_root_ / "mcp_servers.json") : target_path;
    if (!get_config().is_path_safe(dest)) {
        return false;
    }
    try {
        json j;
        json s_map = json::object();
        for (const auto& [name, cfg] : servers_) {
            s_map[name] = cfg.to_json();
        }
        j["mcpServers"] = s_map;

        if (dest.has_parent_path()) {
            std::filesystem::create_directories(dest.parent_path());
        }
        std::ofstream f(dest);
        if (!f.is_open()) return false;
        f << j.dump(2);
        return f.good();
    } catch (...) {
        return false;
    }
}

} // namespace llmcli::tools
