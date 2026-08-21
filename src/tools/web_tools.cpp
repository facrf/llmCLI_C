#include "llmcli/tools/web_tools.hpp"
#include "llmcli/utils/http.hpp"
#include "llmcli/utils/env.hpp"
#include <sstream>
#include <regex>
#include <iostream>

namespace llmcli::tools {

json WebSearchTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Termo de busca a ser pesquisado na internet"}}},
            {"max_results", {{"type", "integer"}, {"description", "Número máximo de resultados (padrão: 5)"}, {"default", 5}}}
        }},
        {"required", json::array({"query"})}
    };
}

ToolResult WebSearchTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string query = arguments.value("query", "");
    int max_results = arguments.value("max_results", 5);

    if (query.empty()) {
        res.success = false;
        res.output = "Argumento 'query' obrigatório.";
        return res;
    }

    std::string tavily_key = utils::EnvLoader::get_env("TAVILY_API_KEY", "");
    if (!tavily_key.empty()) {
        json payload = {
            {"api_key", tavily_key},
            {"query", query},
            {"max_results", max_results}
        };
        auto http_res = utils::HttpClient::post_json("https://api.tavily.com/search", payload, {}, 10.0);
        if (http_res.success) {
            try {
                json data = json::parse(http_res.body);
                if (data.contains("results") && data["results"].is_array()) {
                    std::ostringstream out;
                    out << "Resultados da Web para '" << query << "':\n";
                    for (const auto& r : data["results"]) {
                        out << "- **" << r.value("title", "") << "**: " << r.value("content", "") << "\n  URL: " << r.value("url", "") << "\n\n";
                    }
                    res.success = true;
                    res.output = out.str();
                    return res;
                }
            } catch (...) {}
        }
    }

    // Fallback via DuckDuckGo HTML Lite
    try {
        std::string encoded_q = utils::HttpClient::url_encode(query);
        std::string url = "https://html.duckduckgo.com/html/?q=" + encoded_q;
        auto http_res = utils::HttpClient::get(url, {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}}, 10.0);

        if (http_res.success && !http_res.body.empty()) {
            std::regex snippet_regex(R"(<a class="result__snippet[^>]*>([\s\S]*?)</a>)");
            auto s_begin = std::sregex_iterator(http_res.body.begin(), http_res.body.end(), snippet_regex);
            auto s_end = std::sregex_iterator();

            std::vector<std::string> snippets;
            for (auto it = s_begin; it != s_end; ++it) {
                std::string snip = (*it)[1].str();
                // Strip tags
                snip = std::regex_replace(snip, std::regex("<[^>]+>"), "");
                // Replace whitespace
                snip = std::regex_replace(snip, std::regex(R"(\s+)"), " ");
                snippets.push_back(snip);
                if (static_cast<int>(snippets.size()) >= max_results) break;
            }

            if (!snippets.empty()) {
                std::ostringstream out;
                out << "Resultados da Web para '" << query << "':\n\n";
                for (size_t i = 0; i < snippets.size(); ++i) {
                    out << (i + 1) << ". " << snippets[i] << "\n\n";
                }
                res.success = true;
                res.output = out.str();
                return res;
            }
        }
    } catch (...) {}

    res.success = true;
    res.output = "Busca realizada para: '" + query + "'. Para melhores resultados na web, configure TAVILY_API_KEY no arquivo .env.";
    return res;
}

// ReadUrlTool
json ReadUrlTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {{"type", "string"}, {"description", "URL HTTP ou HTTPS completa para leitura"}}},
            {"max_chars", {{"type", "integer"}, {"description", "Limite máximo de caracteres a extrair (padrão: 6000)"}, {"default", 6000}}}
        }},
        {"required", json::array({"url"})}
    };
}

ToolResult ReadUrlTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string url = arguments.value("url", "");
    int max_chars = arguments.value("max_chars", 6000);

    if (url.empty()) {
        res.success = false;
        res.output = "Argumento 'url' obrigatório.";
        return res;
    }

    auto http_res = utils::HttpClient::get(url, {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}}, 15.0);
    if (!http_res.success) {
        res.success = false;
        res.output = "Erro HTTP " + std::to_string(http_res.status_code) + " ao acessar " + url + ": " + http_res.error;
        return res;
    }

    std::string html = http_res.body;
    // Strip scripts and styles
    html = std::regex_replace(html, std::regex(R"(<(script|style)[^>]*>[\s\S]*?</\1>)", std::regex::icase), " ");
    // Strip tags
    html = std::regex_replace(html, std::regex("<[^>]+>"), " ");
    // Replace multiple spaces
    html = std::regex_replace(html, std::regex(R"(\s+)"), " ");

    if (static_cast<int>(html.size()) > max_chars) {
        html = html.substr(0, max_chars) + "\n... [conteúdo truncado em " + std::to_string(max_chars) + " caracteres]";
    }

    res.success = true;
    res.output = "Conteúdo extraído de " + url + ":\n\n" + html;
    return res;
}

} // namespace llmcli::tools
