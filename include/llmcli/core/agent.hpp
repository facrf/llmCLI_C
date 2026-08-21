#pragma once

#include "llmcli/core/session.hpp"
#include "llmcli/providers/base.hpp"
#include "llmcli/tools/base.hpp"
#include "llmcli/context/semantic_indexer.hpp"
#include "llmcli/tools/mcp_client.hpp"
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace llmcli::core {

class Agent {
public:
    explicit Agent(
        std::shared_ptr<Session> session = nullptr,
        std::shared_ptr<providers::LLMProvider> architect_provider = nullptr
    );

    void set_model(const std::string& model_name);
    void refresh_provider();

    std::string run_prompt(const std::string& user_prompt, int max_iterations = 8);

    ToolResult execute_tool_with_permission(
        const std::string& tool_name,
        const json& arguments,
        const std::string& tool_call_id = ""
    );

    std::vector<ToolDefinition> get_tool_definitions() const;

    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

    context::SemanticIndexer& indexer() { return *indexer_; }
    tools::McpManager& mcp_manager() { return *mcp_manager_; }
    std::map<std::string, std::shared_ptr<tools::BaseTool>>& tools() { return tools_; }

private:
    std::shared_ptr<Session> session_;
    std::shared_ptr<providers::LLMProvider> provider_;
    std::shared_ptr<providers::LLMProvider> architect_provider_;
    std::shared_ptr<context::SemanticIndexer> indexer_;
    std::shared_ptr<tools::McpManager> mcp_manager_;

    std::map<std::string, std::shared_ptr<tools::BaseTool>> tools_;

    void print_token_usage(
        const std::vector<ChatMessage>& messages,
        const std::string& response_text,
        std::optional<int> native_prompt_tokens = std::nullopt,
        std::optional<int> native_completion_tokens = std::nullopt
    );
    std::shared_ptr<providers::LLMProvider> get_architect_provider();

    std::string run_architect_pipeline(const std::string& user_prompt, int max_iterations = 8);
    std::string run_editor_loop(int max_iterations = 8);
};

} // namespace llmcli::core
