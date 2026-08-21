#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <memory>
#include <map>

namespace llmcli::tools {

class BaseTool {
public:
    virtual ~BaseTool() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual json parameters_schema() const = 0;

    virtual ToolResult execute(const json& arguments) = 0;

    virtual ToolDefinition get_definition() const {
        return ToolDefinition{
            name(),
            description(),
            parameters_schema()
        };
    }
};

} // namespace llmcli::tools
