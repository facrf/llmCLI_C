#include "llmcli/context/file_tracker.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace llmcli::context {

std::pair<bool, std::string> FileTracker::add_file(const std::string& file_path) {
    Config& cfg = get_config();
    std::filesystem::path p(file_path);
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
        return {false, "Caminho fora do workspace permitido: " + file_path};
    }

    if (!std::filesystem::exists(resolved)) {
        return {false, "Arquivo não encontrado: " + file_path};
    }

    if (std::filesystem::is_directory(resolved)) {
        int added = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(resolved, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file()) {
                    std::string entry_str = entry.path().string();
                    if (entry_str.find("/.git") != std::string::npos ||
                        entry_str.find("/__pycache__") != std::string::npos ||
                        entry_str.find("/node_modules") != std::string::npos ||
                        entry_str.find("/.venv") != std::string::npos ||
                        entry_str.find("/build") != std::string::npos ||
                        entry_str.find("/.cache") != std::string::npos) {
                        continue;
                    }
                    std::string rel = std::filesystem::relative(entry.path(), cfg.project_root).string();
                    tracked_files_.insert(rel);
                    added++;
                }
            }
        } catch (const std::exception& e) {
            return {false, std::string("Erro ao ler diretório: ") + e.what()};
        }
        return {true, std::to_string(added) + " arquivos do diretório '" + file_path + "' adicionados ao contexto."};
    }

    std::string rel = std::filesystem::relative(resolved, cfg.project_root).string();
    tracked_files_.insert(rel);
    return {true, "Arquivo '" + rel + "' adicionado ao contexto."};
}

bool FileTracker::remove_file(const std::string& file_path) {
    auto it = tracked_files_.find(file_path);
    if (it != tracked_files_.end()) {
        tracked_files_.erase(it);
        return true;
    }
    // Also check if relative to project root
    Config& cfg = get_config();
    std::filesystem::path p(file_path);
    if (p.is_absolute()) {
        std::string rel = std::filesystem::relative(p, cfg.project_root).string();
        auto it_rel = tracked_files_.find(rel);
        if (it_rel != tracked_files_.end()) {
            tracked_files_.erase(it_rel);
            return true;
        }
    }
    return false;
}

void FileTracker::clear() {
    tracked_files_.clear();
}

std::vector<std::string> FileTracker::list_files() const {
    return std::vector<std::string>(tracked_files_.begin(), tracked_files_.end());
}

std::string FileTracker::get_context_text() const {
    if (tracked_files_.empty()) return "";

    Config& cfg = get_config();
    std::ostringstream out;
    out << "=== ARQUIVOS ATIVOS NO CONTEXTO ===";

    for (const auto& rel_path : tracked_files_) {
        std::filesystem::path full_path = cfg.project_root / rel_path;
        if (!std::filesystem::exists(full_path) || !std::filesystem::is_regular_file(full_path)) {
            continue;
        }

        out << "\n--- Início de '" << rel_path << "' ---\n";
        std::ifstream file(full_path);
        if (file.is_open()) {
            std::string line;
            int line_num = 1;
            while (std::getline(file, line)) {
                out << std::setw(4) << line_num << " | " << line << "\n";
                line_num++;
            }
        } else {
            out << "--- (erro ao ler arquivo) ---\n";
        }
        out << "--- Fim de '" << rel_path << "' ---";
    }

    out << "\n===================================";
    return out.str();
}

} // namespace llmcli::context
