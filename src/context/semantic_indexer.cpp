#include "llmcli/context/semantic_indexer.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <regex>
#include <algorithm>
#include <iostream>

namespace llmcli::context {

static const std::set<std::string> SUPPORTED_EXTENSIONS = {
    ".cpp", ".hpp", ".cc", ".c", ".h", ".py", ".js", ".ts", ".jsx", ".tsx",
    ".go", ".rs", ".java", ".php", ".rb", ".md", ".json", ".yaml", ".yml", ".sh"
};

static const std::set<std::string> IGNORE_DIRS = {
    ".git", ".venv", "venv", "__pycache__", "node_modules", "dist",
    "build", ".pytest_cache", ".cache", "bin", "obj"
};

SemanticIndexer::SemanticIndexer(const std::filesystem::path& project_root) {
    if (project_root.empty()) {
        project_root_ = get_config().project_root;
    } else {
        project_root_ = project_root;
    }
    cache_path_ = project_root_ / ".cache" / "semantic_index.json";
}

std::vector<std::string> SemanticIndexer::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            // Check for camelCase split
            if (std::isupper(static_cast<unsigned char>(c)) && !current.empty() && std::islower(static_cast<unsigned char>(current.back()))) {
                std::string lower = current;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch){ return std::tolower(ch); });
                tokens.push_back(lower);
                current.clear();
            }
            current += c;
        } else {
            if (!current.empty()) {
                std::string lower = current;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch){ return std::tolower(ch); });
                tokens.push_back(lower);
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        std::string lower = current;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch){ return std::tolower(ch); });
        tokens.push_back(lower);
    }
    return tokens;
}

std::vector<CodeChunk> SemanticIndexer::chunk_file(const std::string& rel_path, const std::string& content) const {
    std::vector<CodeChunk> file_chunks;
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) return file_chunks;

    const size_t chunk_size = 50;
    const size_t overlap = 10;

    for (size_t i = 0; i < lines.size(); i += (chunk_size - overlap)) {
        size_t end_idx = std::min(lines.size(), i + chunk_size);
        std::ostringstream chunk_ss;
        for (size_t j = i; j < end_idx; ++j) {
            chunk_ss << lines[j] << "\n";
        }

        std::string symbol_name = std::filesystem::path(rel_path).filename().string() + ":" + std::to_string(i + 1);

        CodeChunk c;
        c.file_path = rel_path;
        c.symbol_name = symbol_name;
        c.symbol_type = "block";
        c.start_line = static_cast<int>(i + 1);
        c.end_line = static_cast<int>(end_idx);
        c.content = chunk_ss.str();
        file_chunks.push_back(c);

        if (end_idx == lines.size()) break;
    }

    return file_chunks;
}

int SemanticIndexer::index_codebase() {
    chunks_.clear();
    doc_freq_.clear();
    doc_lengths_.clear();

    if (!std::filesystem::exists(project_root_)) return 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(project_root_)) {
            if (entry.is_directory()) {
                std::string dname = entry.path().filename().string();
                if (IGNORE_DIRS.find(dname) != IGNORE_DIRS.end() || dname[0] == '.') {
                    continue;
                }
            } else if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::string path_str = entry.path().string();

                bool ignored = false;
                for (const auto& ig : IGNORE_DIRS) {
                    if (path_str.find("/" + ig + "/") != std::string::npos) {
                        ignored = true;
                        break;
                    }
                }
                if (ignored) continue;

                if (SUPPORTED_EXTENSIONS.find(ext) != SUPPORTED_EXTENSIONS.end()) {
                    std::ifstream f(entry.path());
                    if (f.is_open()) {
                        std::stringstream buffer;
                        buffer << f.rdbuf();
                        std::string content = buffer.str();
                        if (!content.empty()) {
                            std::string rel = std::filesystem::relative(entry.path(), project_root_).string();
                            auto file_chunks = chunk_file(rel, content);
                            chunks_.insert(chunks_.end(), file_chunks.begin(), file_chunks.end());
                        }
                    }
                }
            }
        }
    } catch (...) {}

    // Calculate BM25 statistics
    int total_docs = static_cast<int>(chunks_.size());
    long long total_len = 0;

    for (const auto& chunk : chunks_) {
        auto tokens = tokenize(chunk.symbol_name + " " + chunk.content);
        doc_lengths_.push_back(static_cast<int>(tokens.size()));
        total_len += tokens.size();

        std::set<std::string> unique_tokens(tokens.begin(), tokens.end());
        for (const auto& t : unique_tokens) {
            doc_freq_[t]++;
        }
    }

    avg_doc_len_ = total_docs > 0 ? static_cast<double>(total_len) / total_docs : 1.0;
    save_index();
    return total_docs;
}

std::vector<SearchResult> SemanticIndexer::search(const std::string& query, int top_k) {
    if (chunks_.empty()) {
        if (!load_index()) {
            index_codebase();
        }
    }

    auto query_tokens = tokenize(query);
    if (query_tokens.empty() || chunks_.empty()) return {};

    int N = static_cast<int>(chunks_.size());
    const double k1 = 1.5;
    const double b = 0.75;

    std::vector<std::pair<int, double>> scores;

    for (size_t idx = 0; idx < chunks_.size(); ++idx) {
        const auto& chunk = chunks_[idx];
        auto doc_tokens = tokenize(chunk.symbol_name + " " + chunk.content);
        int doc_len = static_cast<int>(doc_tokens.size());

        std::map<std::string, int> tf_map;
        for (const auto& tok : doc_tokens) {
            tf_map[tok]++;
        }

        double score = 0.0;
        for (const auto& q : query_tokens) {
            auto tf_it = tf_map.find(q);
            if (tf_it == tf_map.end() || tf_it->second == 0) continue;

            int tf = tf_it->second;
            int df = 1;
            auto df_it = doc_freq_.find(q);
            if (df_it != doc_freq_.end()) {
                df = df_it->second;
            }

            double idf = std::log(1.0 + (N - df + 0.5) / (df + 0.5));
            double num = tf * (k1 + 1.0);
            double den = tf + k1 * (1.0 - b + b * (static_cast<double>(doc_len) / std::max(1.0, avg_doc_len_)));

            score += idf * (num / std::max(1.0, den));

            // Bonus if term appears in symbol name or file path
            std::string sym_lower = chunk.symbol_name;
            std::transform(sym_lower.begin(), sym_lower.end(), sym_lower.begin(), [](unsigned char c){ return std::tolower(c); });
            if (sym_lower.find(q) != std::string::npos) {
                score += 2.5;
            }
        }

        if (score > 0.0) {
            scores.emplace_back(static_cast<int>(idx), score);
        }
    }

    std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b){
        return a.second > b.second;
    });

    std::vector<SearchResult> results;
    for (size_t i = 0; i < std::min(static_cast<size_t>(top_k), scores.size()); ++i) {
        results.push_back({chunks_[scores[i].first], scores[i].second});
    }

    return results;
}

bool SemanticIndexer::save_index() {
    try {
        if (cache_path_.has_parent_path()) {
            std::filesystem::create_directories(cache_path_.parent_path());
        }
        json j;
        j["total_docs"] = chunks_.size();
        json chunks_json = json::array();
        for (const auto& c : chunks_) {
            chunks_json.push_back(c.to_json());
        }
        j["chunks"] = chunks_json;

        std::ofstream f(cache_path_);
        f << j.dump();
        return true;
    } catch (...) {
        return false;
    }
}

bool SemanticIndexer::load_index() {
    if (!std::filesystem::exists(cache_path_)) return false;
    try {
        std::ifstream f(cache_path_);
        json j;
        f >> j;
        if (!j.contains("chunks") || !j["chunks"].is_array()) return false;

        chunks_.clear();
        doc_freq_.clear();
        doc_lengths_.clear();

        for (const auto& item : j["chunks"]) {
            chunks_.push_back(CodeChunk::from_json(item));
        }

        long long total_len = 0;
        for (const auto& chunk : chunks_) {
            auto tokens = tokenize(chunk.symbol_name + " " + chunk.content);
            doc_lengths_.push_back(static_cast<int>(tokens.size()));
            total_len += tokens.size();
            std::set<std::string> unique_tokens(tokens.begin(), tokens.end());
            for (const auto& t : unique_tokens) {
                doc_freq_[t]++;
            }
        }
        avg_doc_len_ = chunks_.empty() ? 1.0 : static_cast<double>(total_len) / chunks_.size();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace llmcli::context
