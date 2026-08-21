#include "llmcli/providers/gemini.hpp"
#include "llmcli/utils/http.hpp"
#include "llmcli/utils/env.hpp"
#include <iostream>

namespace llmcli::providers {

GeminiProvider::GeminiProvider(const std::string& model_name, const std::string& api_key)
    : LLMProvider(model_name) {
    if (model_name_.rfind("gemini/", 0) == 0) {
        model_name_ = model_name_.substr(7);
    }
    api_key_ = api_key.empty() ? utils::EnvLoader::get_env("GEMINI_API_KEY", "") : api_key;
}

std::pair<json, json> GeminiProvider::convert_contents(const std::vector<ChatMessage>& messages) const {
    json system_instruction = nullptr;
    json contents = json::array();

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            system_instruction = {
                {"parts", json::array({{{"text", msg.content}}})}
            };
        } else if (msg.role == "user") {
            if (!contents.empty() && contents.back()["role"] == "user") {
                contents.back()["parts"].push_back({{"text", msg.content}});
            } else {
                contents.push_back({
                    {"role", "user"},
                    {"parts", json::array({{{"text", msg.content}}})}
                });
            }
        } else if (msg.role == "assistant") {
            json parts = json::array();
            if (!msg.content.empty()) {
                parts.push_back({{"text", msg.content}});
            }
            if (!msg.tool_calls.empty()) {
                for (const auto& tc : msg.tool_calls) {
                    parts.push_back({
                        {"functionCall", {
                            {"name", tc.name},
                            {"args", tc.arguments.is_object() ? tc.arguments : json::object()}
                        }}
                    });
                }
            }
            if (parts.empty()) {
                parts.push_back({{"text", ""}});
            }
            contents.push_back({
                {"role", "model"},
                {"parts", parts}
            });
        } else if (msg.role == "tool") {
            json func_response_part = {
                {"functionResponse", {
                    {"name", msg.name.empty() ? "tool_call" : msg.name},
                    {"response", {{"output", msg.content}}}
                }}
            };
            if (!contents.empty() && contents.back()["role"] == "user") {
                contents.back()["parts"].push_back(func_response_part);
            } else {
                contents.push_back({
                    {"role", "user"},
                    {"parts", json::array({func_response_part})}
                });
            }
        }
    }

    return {system_instruction, contents};
}

json GeminiProvider::convert_tools(const std::vector<ToolDefinition>& tools) const {
    json decls = json::array();
    for (const auto& t : tools) {
        decls.push_back({
            {"name", t.name},
            {"description", t.description},
            {"parameters", t.parameters}
        });
    }
    return json::array({{{"functionDeclarations", decls}}});
}

bool GeminiProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::vector<ToolDefinition>& tools,
    std::function<void(const StreamChunk& chunk)> on_chunk,
    double temperature,
    int max_tokens
) {
    if (api_key_.empty()) {
        StreamChunk err;
        err.is_done = true;
        err.error = "Chave GEMINI_API_KEY não configurada no arquivo .env.";
        on_chunk(err);
        return false;
    }

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_name_ + ":streamGenerateContent?alt=sse&key=" + api_key_;

    auto [sys_inst, contents] = convert_contents(messages);

    json payload = {
        {"contents", contents},
        {"generationConfig", {
            {"temperature", temperature},
            {"maxOutputTokens", max_tokens}
        }}
    };

    if (!sys_inst.is_null()) {
        payload["systemInstruction"] = sys_inst;
    }

    if (!tools.empty()) {
        payload["tools"] = convert_tools(tools);
    }

    std::vector<ToolCall> accumulated_calls;
    std::string out_err;
    std::optional<int> last_prompt_tokens;
    std::optional<int> last_completion_tokens;

    bool success = utils::HttpClient::post_stream(
        url,
        payload,
        {},
        [&](const std::string& line) {
            std::string l = line;
            auto f = l.find_first_not_of(" \t\r\n");
            if (f == std::string::npos) return;
            l = l.substr(f);

            if (l.rfind("data: ", 0) == 0) {
                std::string raw_json = l.substr(6);
                try {
                    json data = json::parse(raw_json);
                    if (data.contains("usageMetadata") && data["usageMetadata"].is_object()) {
                        auto um = data["usageMetadata"];
                        if (um.contains("promptTokenCount") && um["promptTokenCount"].is_number()) {
                            last_prompt_tokens = um["promptTokenCount"].get<int>();
                        }
                        if (um.contains("candidatesTokenCount") && um["candidatesTokenCount"].is_number()) {
                            last_completion_tokens = um["candidatesTokenCount"].get<int>();
                        }
                    }

                    if (data.contains("candidates") && data["candidates"].is_array() && !data["candidates"].empty()) {
                        auto cand = data["candidates"][0];
                        if (cand.contains("content") && cand["content"].contains("parts") && cand["content"]["parts"].is_array()) {
                            for (const auto& part : cand["content"]["parts"]) {
                                if (part.contains("text") && part["text"].is_string()) {
                                    StreamChunk ch;
                                    ch.delta_content = part["text"].get<std::string>();
                                    ch.prompt_tokens = last_prompt_tokens;
                                    ch.completion_tokens = last_completion_tokens;
                                    on_chunk(ch);
                                } else if (part.contains("functionCall") && part["functionCall"].is_object()) {
                                    auto fc = part["functionCall"];
                                    std::string name = fc.value("name", "");
                                    json args = fc.contains("args") ? fc["args"] : json::object();
                                    accumulated_calls.push_back({
                                        "gemini_call_" + std::to_string(accumulated_calls.size()),
                                        name,
                                        args
                                    });
                                }
                            }
                        }
                    }
                } catch (...) {}
            }
        },
        out_err,
        120.0
    );

    if (!success) {
        StreamChunk err;
        err.is_done = true;
        err.error = "Erro na requisição Gemini: " + out_err;
        on_chunk(err);
        return false;
    }

    if (!accumulated_calls.empty()) {
        StreamChunk tc_chunk;
        tc_chunk.tool_calls = accumulated_calls;
        tc_chunk.is_done = true;
        tc_chunk.prompt_tokens = last_prompt_tokens;
        tc_chunk.completion_tokens = last_completion_tokens;
        on_chunk(tc_chunk);
    } else {
        StreamChunk done_chunk;
        done_chunk.is_done = true;
        done_chunk.prompt_tokens = last_prompt_tokens;
        done_chunk.completion_tokens = last_completion_tokens;
        on_chunk(done_chunk);
    }

    return true;
}

std::pair<bool, std::string> GeminiProvider::check_health() {
    if (api_key_.empty()) {
        return {false, "GEMINI_API_KEY ausente no .env"};
    }
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_name_ + "?key=" + api_key_;
    auto res = utils::HttpClient::get(url, {}, 5.0);
    if (res.status_code == 200) {
        return {true, "API Google Gemini conectada"};
    }
    return {false, "HTTP " + std::to_string(res.status_code)};
}

} // namespace llmcli::providers
