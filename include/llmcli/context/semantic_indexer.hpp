#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <set>

namespace llmcli::context {

class SemanticIndexer {
public:
    explicit SemanticIndexer(const std::filesystem::path& project_root = "");

    int index_codebase();
    std::vector<SearchResult> search(const std::string& query, int top_k = 5);

    bool save_index();
    bool load_index();

    const std::vector<CodeChunk>& chunks() const { return chunks_; }
    size_t total_docs() const { return chunks_.size(); }

private:
    std::filesystem::path project_root_;
    std::filesystem::path cache_path_;
    std::vector<CodeChunk> chunks_;

    std::map<std::string, int> doc_freq_;
    std::vector<int> doc_lengths_;
    double avg_doc_len_{0.0};

    std::vector<std::string> tokenize(const std::string& text) const;
    std::vector<CodeChunk> chunk_file(const std::string& rel_path, const std::string& content) const;
};

} // namespace llmcli::context
