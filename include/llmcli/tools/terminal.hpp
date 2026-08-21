#pragma once

#include "llmcli/tools/base.hpp"

namespace llmcli::tools {

class RunCommandTool : public BaseTool {
public:
    std::string name() const override { return "run_command"; }
    std::string description() const override {
        return "Executa um comando shell no terminal dentro da raiz do workspace.";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

} // namespace llmcli::tools
