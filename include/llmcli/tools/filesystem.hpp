#pragma once

#include "llmcli/tools/base.hpp"
#include <filesystem>

namespace llmcli::tools {

std::filesystem::path resolve_safe_path(const std::string& target_path);

class ReadFileTool : public BaseTool {
public:
    std::string name() const override { return "read_file"; }
    std::string description() const override {
        return "Lê o conteúdo de um arquivo de texto, opcionalmente entre um intervalo de linhas (1-indexed).";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

class WriteFileTool : public BaseTool {
public:
    std::string name() const override { return "write_file"; }
    std::string description() const override {
        return "Escreve ou substitui o conteúdo completo de um arquivo dentro do workspace.";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

class ListDirTool : public BaseTool {
public:
    std::string name() const override { return "list_dir"; }
    std::string description() const override {
        return "Lista diretórios e arquivos de um caminho do workspace.";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

class GrepSearchTool : public BaseTool {
public:
    std::string name() const override { return "grep_search"; }
    std::string description() const override {
        return "Busca ocorrências de texto ou regex dentro dos arquivos do projeto.";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

class FindFilesTool : public BaseTool {
public:
    std::string name() const override { return "find_files"; }
    std::string description() const override {
        return "Encontra arquivos pelo nome ou padrão glob (ex: '*.cpp', '*agent*').";
    }
    json parameters_schema() const override;
    ToolResult execute(const json& arguments) override;
};

} // namespace llmcli::tools
