#include "llmcli/context/semantic_indexer.hpp"
#include "llmcli/config.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>

extern void run_test(const std::string& name, const std::function<void()>& fn);

void test_semantic_indexer_suite() {
    run_test("Indexação e Busca Semântica BM25", [](){
        auto& cfg = llmcli::get_config();
        llmcli::context::SemanticIndexer indexer(cfg.project_root);
        int docs = indexer.index_codebase();
        if (docs < 0) throw std::runtime_error("Número de documentos negativo");

        auto results = indexer.search("config", 3);
        // Should find config related code if codebase has config
    });
}
