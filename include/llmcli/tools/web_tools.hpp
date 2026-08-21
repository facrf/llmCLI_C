#pragma once

#include "llmcli/tools/base.hpp"

namespace llmcli::tools {

class WebSearchTool : public BaseTool {
public:
    std::string name() const override { return "web_search"; }
    std::string description() const override {
        return "Pesquisa informações atualizadas, documentações de APIs, bibliotecas e soluções de erros na Web.";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

class ReadUrlTool : public BaseTool {
public:
    std::string name() const override { return "read_url"; }
    std::string description() const override {
        return "Lê e extrai o conteúdo de texto legível a partir de uma URL da web (ex: documentações, artigos).";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

} // namespace llmcli::tools
