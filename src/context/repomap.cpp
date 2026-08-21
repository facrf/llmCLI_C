#include "llmcli/context/repomap.hpp"
#include <sstream>
#include <vector>
#include <algorithm>
#include <set>

namespace llmcli::context {

static const std::set<std::string> IGNORED_DIRS = {
    ".git", "__pycache__", "node_modules", ".venv", "env", ".cache", "dist", "build", "bin", "obj"
};

std::string build_repo_map(const std::filesystem::path& project_root, int max_files) {
    if (!std::filesystem::exists(project_root) || !std::filesystem::is_directory(project_root)) {
        return "(Diretório do projeto não encontrado)";
    }

    std::ostringstream out;
    std::string root_name = project_root.filename().string();
    if (root_name.empty()) root_name = project_root.string();

    out << "Estrutura do Projeto (" << root_name << "):\n";
    int count = 0;

    auto print_dir = [&](auto& self, const std::filesystem::path& dir, int depth) -> void {
        if (depth > 4 || count >= max_files) return;

        std::vector<std::filesystem::path> subdirs;
        std::vector<std::filesystem::path> files;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                std::string fname = entry.path().filename().string();
                if (entry.is_directory()) {
                    if (IGNORED_DIRS.find(fname) == IGNORED_DIRS.end() && fname[0] != '.') {
                        subdirs.push_back(entry.path());
                    }
                } else if (entry.is_regular_file()) {
                    if (fname[0] != '.' || fname == ".env.example" || fname == ".gitignore") {
                        files.push_back(entry.path());
                    }
                }
            }
        } catch (...) {
            return;
        }

        std::sort(subdirs.begin(), subdirs.end());
        std::sort(files.begin(), files.end());

        std::string indent(depth * 2, ' ');

        for (const auto& d : subdirs) {
            out << indent << "📁 " << d.filename().string() << "/\n";
            self(self, d, depth + 1);
            if (count >= max_files) return;
        }

        for (const auto& f : files) {
            out << indent << "📄 " << f.filename().string() << "\n";
            count++;
            if (count >= max_files) {
                out << indent << "... (árvore truncada para brevidade)\n";
                return;
            }
        }
    };

    print_dir(print_dir, project_root, 1);
    return out.str();
}

} // namespace llmcli::context
