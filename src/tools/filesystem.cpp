#include "llmcli/tools/filesystem.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <iomanip>
#include <fnmatch.h>
#include <algorithm>

namespace llmcli::tools {

std::filesystem::path resolve_safe_path(const std::string& target_path) {
    Config& cfg = get_config();
    std::filesystem::path p(target_path);
    std::filesystem::path resolved;

    if (!p.is_absolute()) {
        resolved = cfg.project_root / p;
    } else {
        resolved = p;
    }

    try {
        resolved = std::filesystem::weakly_canonical(resolved);
    } catch (...) {}

    if (!cfg.is_path_safe(resolved)) {
        throw std::runtime_error("Acesso negado: O caminho '" + target_path + "' está fora da raiz permitida do workspace (" + cfg.project_root.string() + ").");
    }
    return resolved;
}

// ReadFileTool
json ReadFileTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Caminho relativo ou absoluto do arquivo no workspace."}}},
            {"start_line", {{"type", "integer"}, {"description", "Linha inicial para leitura (1-indexed, opcional)."}}},
            {"end_line", {{"type", "integer"}, {"description", "Linha final para leitura (inclusive, opcional)."}}}
        }},
        {"required", json::array({"path"})}
    };
}

ToolResult ReadFileTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string path_str = arguments.value("path", "");
    if (path_str.empty()) {
        res.success = false;
        res.output = "Argumento 'path' obrigatório.";
        return res;
    }

    try {
        auto safe_path = resolve_safe_path(path_str);
        if (!std::filesystem::exists(safe_path)) {
            res.success = false;
            res.output = "Arquivo não encontrado: " + path_str;
            return res;
        }
        if (!std::filesystem::is_regular_file(safe_path)) {
            res.success = false;
            res.output = "O caminho não é um arquivo: " + path_str;
            return res;
        }

        std::ifstream file(safe_path);
        if (!file.is_open()) {
            res.success = false;
            res.output = "Não foi possível abrir o arquivo: " + path_str;
            return res;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }

        int total_lines = static_cast<int>(lines.size());
        int start_line = arguments.value("start_line", 1);
        int end_line = arguments.value("end_line", total_lines);

        start_line = std::max(1, std::min(start_line, total_lines));
        end_line = std::max(start_line, std::min(end_line, total_lines));

        std::ostringstream out;
        out << "--- Arquivo: " << path_str << " (Linhas " << start_line << "-" << end_line << " de " << total_lines << ") ---\n";
        for (int i = start_line - 1; i < end_line && i < total_lines; ++i) {
            out << std::setw(4) << (i + 1) << " | " << lines[i] << "\n";
        }

        res.success = true;
        res.output = out.str();
        return res;

    } catch (const std::exception& e) {
        res.success = false;
        res.output = "Erro ao ler arquivo: " + std::string(e.what());
        return res;
    }
}

// WriteFileTool
json WriteFileTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Caminho do arquivo a ser criado ou substituído."}}},
            {"content", {{"type", "string"}, {"description", "Conteúdo textual completo do arquivo."}}}
        }},
        {"required", json::array({"path", "content"})}
    };
}

ToolResult WriteFileTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string path_str = arguments.value("path", "");
    std::string content = arguments.value("content", "");

    if (path_str.empty()) {
        res.success = false;
        res.output = "Argumento 'path' obrigatório.";
        return res;
    }

    try {
        auto safe_path = resolve_safe_path(path_str);
        if (safe_path.has_parent_path()) {
            std::filesystem::create_directories(safe_path.parent_path());
        }

        std::ofstream file(safe_path);
        if (!file.is_open()) {
            res.success = false;
            res.output = "Não foi possível abrir o arquivo para gravação: " + path_str;
            return res;
        }

        file << content;
        file.close();

        res.success = true;
        res.output = "Arquivo gravado com sucesso: " + path_str + " (" + std::to_string(content.size()) + " caracteres)";
        return res;

    } catch (const std::exception& e) {
        res.success = false;
        res.output = "Erro ao gravar arquivo: " + std::string(e.what());
        return res;
    }
}

// ListDirTool
json ListDirTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Caminho do diretório (padrão: '.')."}, {"default", "."}}},
            {"max_depth", {{"type", "integer"}, {"description", "Profundidade máxima de listagem (padrão: 2)."}, {"default", 2}}}
        }},
        {"required", json::array()}
    };
}

ToolResult ListDirTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string path_str = arguments.value("path", ".");
    int max_depth = arguments.value("max_depth", 2);

    try {
        auto safe_path = resolve_safe_path(path_str);
        if (!std::filesystem::exists(safe_path) || !std::filesystem::is_directory(safe_path)) {
            res.success = false;
            res.output = "Diretório inválido: " + path_str;
            return res;
        }

        std::ostringstream out;
        out << "Conteúdo de '" << path_str << "':\n";

        auto list_rec = [&](auto& self, const std::filesystem::path& dir, int depth) -> void {
            if (depth > max_depth) return;

            std::vector<std::filesystem::path> subdirs;
            std::vector<std::filesystem::path> files;

            try {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string fname = entry.path().filename().string();
                    if (fname == ".git" || fname == "__pycache__" || fname == "node_modules" || fname == ".venv" || fname == "build") {
                        continue;
                    }
                    if (entry.is_directory()) {
                        subdirs.push_back(entry.path());
                    } else if (entry.is_regular_file()) {
                        files.push_back(entry.path());
                    }
                }
            } catch (...) { return; }

            std::sort(subdirs.begin(), subdirs.end());
            std::sort(files.begin(), files.end());

            std::string indent(depth * 2, ' ');

            for (const auto& d : subdirs) {
                out << indent << "📁 " << d.filename().string() << "/\n";
                self(self, d, depth + 1);
            }
            for (const auto& f : files) {
                out << indent << "📄 " << f.filename().string() << "\n";
            }
        };

        list_rec(list_rec, safe_path, 0);

        res.success = true;
        res.output = out.str();
        return res;

    } catch (const std::exception& e) {
        res.success = false;
        res.output = "Erro ao listar diretório: " + std::string(e.what());
        return res;
    }
}

// GrepSearchTool
json GrepSearchTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Termo ou padrão regex de busca."}}},
            {"path", {{"type", "string"}, {"description", "Diretório ou arquivo alvo (padrão: '.')."}, {"default", "."}}},
            {"case_sensitive", {{"type", "boolean"}, {"description", "Diferencia maiúsculas de minúsculas."}, {"default", false}}}
        }},
        {"required", json::array({"query"})}
    };
}

ToolResult GrepSearchTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string query = arguments.value("query", "");
    std::string path_str = arguments.value("path", ".");
    bool case_sensitive = arguments.value("case_sensitive", false);

    if (query.empty()) {
        res.success = false;
        res.output = "Argumento 'query' obrigatório.";
        return res;
    }

    try {
        auto safe_path = resolve_safe_path(path_str);
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (!case_sensitive) {
            flags |= std::regex::icase;
        }
        std::regex pattern(query, flags);

        std::vector<std::string> matches;
        Config& cfg = get_config();

        auto search_file = [&](const std::filesystem::path& file_p) {
            std::ifstream f(file_p);
            if (!f.is_open()) return;

            std::string line;
            int line_num = 1;
            std::string rel = std::filesystem::relative(file_p, cfg.project_root).string();

            while (std::getline(f, line)) {
                if (std::regex_search(line, pattern)) {
                    matches.push_back(rel + ":" + std::to_string(line_num) + ": " + line);
                    if (matches.size() >= 50) break;
                }
                line_num++;
            }
        };

        if (std::filesystem::is_regular_file(safe_path)) {
            search_file(safe_path);
        } else if (std::filesystem::is_directory(safe_path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(safe_path, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file()) {
                    std::string pstr = entry.path().string();
                    if (pstr.find("/.git") != std::string::npos ||
                        pstr.find("/__pycache__") != std::string::npos ||
                        pstr.find("/node_modules") != std::string::npos ||
                        pstr.find("/.venv") != std::string::npos ||
                        pstr.find("/build") != std::string::npos) {
                        continue;
                    }
                    search_file(entry.path());
                    if (matches.size() >= 50) break;
                }
            }
        }

        if (matches.empty()) {
            res.success = true;
            res.output = "Nenhuma ocorrência encontrada para '" + query + "'.";
            return res;
        }

        std::ostringstream out;
        for (const auto& m : matches) {
            out << m << "\n";
        }
        if (matches.size() >= 50) {
            out << "... (limite de 50 resultados atingido)\n";
        }

        res.success = true;
        res.output = out.str();
        return res;

    } catch (const std::exception& e) {
        res.success = false;
        res.output = "Erro na busca: " + std::string(e.what());
        return res;
    }
}

// FindFilesTool
json FindFilesTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {{"type", "string"}, {"description", "Padrão de busca glob (ex: '*.cpp', '*agent*')."}}},
            {"path", {{"type", "string"}, {"description", "Diretório de início (padrão: '.')."}, {"default", "."}}}
        }},
        {"required", json::array({"pattern"})}
    };
}

ToolResult FindFilesTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string pattern = arguments.value("pattern", "");
    std::string path_str = arguments.value("path", ".");

    if (pattern.empty()) {
        res.success = false;
        res.output = "Argumento 'pattern' obrigatório.";
        return res;
    }

    try {
        auto safe_path = resolve_safe_path(path_str);
        std::vector<std::string> matches;
        Config& cfg = get_config();

        for (const auto& entry : std::filesystem::recursive_directory_iterator(safe_path, std::filesystem::directory_options::skip_permission_denied)) {
            std::string fname = entry.path().filename().string();
            std::string pstr = entry.path().string();

            if (pstr.find("/.git") != std::string::npos ||
                pstr.find("/__pycache__") != std::string::npos ||
                pstr.find("/node_modules") != std::string::npos ||
                pstr.find("/.venv") != std::string::npos ||
                pstr.find("/build") != std::string::npos) {
                continue;
            }

            if (fnmatch(pattern.c_str(), fname.c_str(), 0) == 0) {
                std::string rel = std::filesystem::relative(entry.path(), cfg.project_root).string();
                matches.push_back(rel);
                if (matches.size() >= 50) break;
            }
        }

        if (matches.empty()) {
            res.success = true;
            res.output = "Nenhum arquivo encontrado com padrão '" + pattern + "'.";
            return res;
        }

        std::ostringstream out;
        for (const auto& m : matches) {
            out << m << "\n";
        }
        res.success = true;
        res.output = out.str();
        return res;

    } catch (const std::exception& e) {
        res.success = false;
        res.output = "Erro ao buscar arquivos: " + std::string(e.what());
        return res;
    }
}

} // namespace llmcli::tools
