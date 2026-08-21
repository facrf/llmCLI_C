#include "llmcli/providers/openai_compatible.hpp"
#include "llmcli/utils/http.hpp"
#include <iostream>
#include <sstream>

namespace llmcli::providers {

OpenAICompatibleProvider::OpenAICompatibleProvider(
    const std::string& model_name,
    const std::string& base_url,
    const std::string& api_key,
    const std::map<std::string, std::string>& extra_headers,
    double timeout_seconds
) : LLMProvider(model_name),
    base_url_(base_url),
    api_key_(api_key.empty() ? "sk-dummy" : api_key),
    headers_(extra_headers),
    timeout_seconds_(timeout_seconds) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

std::map<std::string, std::string> OpenAICompatibleProvider::build_headers() const {
    std::map<std::string, std::string> hdrs = headers_;
    hdrs["Content-Type"] = "application/json";
    if (!api_key_.empty()) {
        hdrs["Authorization"] = "Bearer " + api_key_;
    }
    return hdrs;
}

json OpenAICompatibleProvider::convert_messages(const std::vector<ChatMessage>& messages) const {
    json converted = json::array();
    for (const auto& msg : messages) {
        json item = {
            {"role", msg.role},
            {"content", msg.content}
        };
        if (msg.role == "assistant" && !msg.tool_calls.empty()) {
            json tcs = json::array();
            for (const auto& tc : msg.tool_calls) {
                tcs.push_back(tc.to_json());
            }
            item["tool_calls"] = tcs;
        }
        if (msg.role == "tool") {
            item["tool_call_id"] = msg.tool_call_id.empty() ? "call_default" : msg.tool_call_id;
            if (!msg.name.empty()) {
                item["name"] = msg.name;
            }
        }
        converted.push_back(item);
    }
    return converted;
}

json OpenAICompatibleProvider::convert_tools(const std::vector<ToolDefinition>& tools) const {
    json converted = json::array();
    for (const auto& t : tools) {
        converted.push_back(t.to_json());
    }
    return converted;
}

bool OpenAICompatibleProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::vector<ToolDefinition>& tools,
    std::function<void(const StreamChunk& chunk)> on_chunk,
    double temperature,
    int max_tokens
) {
    std::string url = base_url_ + "/chat/completions";

    json payload = {
        {"model", model_name_},
        {"messages", convert_messages(messages)},
        {"stream", true},
        {"temperature", temperature},
        {"max_tokens", max_tokens}
    };

    if (!tools.empty()) {
        payload["tools"] = convert_tools(tools);
        payload["tool_choice"] = "auto";
    }

    auto hdrs = build_headers();

    struct ToolCallAccumulator {
        std::string id;
        std::string name;
        std::string arguments_str;
    };
    std::map<int, ToolCallAccumulator> tool_accum;

    std::string out_err;
    std::optional<int> last_prompt_tokens;
    std::optional<int> last_completion_tokens;

    bool success = utils::HttpClient::post_stream(
        url,
        payload,
        build_headers(),
        [&](const std::string& line) {
            std::string l = line;
            auto f = l.find_first_not_of(" \t\r\n");
            if (f == std::string::npos) return;
            l = l.substr(f);

            if (l.rfind("data: [DONE]", 0) == 0 || l.rfind(":", 0) == 0) return;

            if (l.rfind("data: ", 0) == 0) {
                std::string raw_json = l.substr(6);
                try {
                    json data = json::parse(raw_json);

                    // Parse usage if provided (OpenAI / vLLM / Ollama stream_options)
                    if (data.contains("usage") && data["usage"].is_object()) {
                        auto u = data["usage"];
                        if (u.contains("prompt_tokens") && u["prompt_tokens"].is_number()) {
                            last_prompt_tokens = u["prompt_tokens"].get<int>();
                        }
                        if (u.contains("completion_tokens") && u["completion_tokens"].is_number()) {
                            last_completion_tokens = u["completion_tokens"].get<int>();
                        }
                    }
                    if (data.contains("prompt_eval_count") && data["prompt_eval_count"].is_number()) {
                        last_prompt_tokens = data["prompt_eval_count"].get<int>();
                    }
                    if (data.contains("eval_count") && data["eval_count"].is_number()) {
                        last_completion_tokens = data["eval_count"].get<int>();
                    }

                    if (data.contains("choices") && data["choices"].is_array() && !data["choices"].empty()) {
                        auto choice = data["choices"][0];
                        std::string delta_text;
                        std::string finish_reason;

                        if (choice.contains("delta") && choice["delta"].is_object()) {
                            auto delta = choice["delta"];
                            if (delta.contains("content") && delta["content"].is_string()) {
                                delta_text = delta["content"].get<std::string>();
                            }
                            if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                                for (const auto& tc_chunk : delta["tool_calls"]) {
                                    int idx = tc_chunk.value("index", 0);
                                    if (tc_chunk.contains("id") && tc_chunk["id"].is_string()) {
                                        tool_accum[idx].id = tc_chunk["id"].get<std::string>();
                                    }
                                    if (tc_chunk.contains("function") && tc_chunk["function"].is_object()) {
                                        auto func = tc_chunk["function"];
                                        if (func.contains("name") && func["name"].is_string()) {
                                            tool_accum[idx].name += func["name"].get<std::string>();
                                        }
                                        if (func.contains("arguments") && func["arguments"].is_string()) {
                                            tool_accum[idx].arguments_str += func["arguments"].get<std::string>();
                                        }
                                    }
                                }
                            }
                        }
                        if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
                            finish_reason = choice["finish_reason"].get<std::string>();
                        }

                        if (!delta_text.empty()) {
                            StreamChunk ch;
                            ch.delta_content = delta_text;
                            ch.finish_reason = finish_reason;
                            ch.is_done = !finish_reason.empty();
                            ch.prompt_tokens = last_prompt_tokens;
                            ch.completion_tokens = last_completion_tokens;
                            on_chunk(ch);
                        }
                    }
                } catch (...) {}
            }
        },
        out_err,
        timeout_seconds_
    );

    if (!success) {
        StreamChunk err_chunk;
        err_chunk.is_done = true;
        err_chunk.error = "Erro de conexão (" + url + "): " + out_err;
        on_chunk(err_chunk);
        return false;
    }

    if (!tool_accum.empty()) {
        std::vector<ToolCall> final_calls;
        for (auto& [idx, acc] : tool_accum) {
            json args_obj;
            try {
                args_obj = json::parse(acc.arguments_str);
            } catch (...) {
                args_obj = {{"raw_arguments", acc.arguments_str}};
            }
            final_calls.push_back({
                acc.id.empty() ? ("call_" + std::to_string(idx)) : acc.id,
                acc.name,
                args_obj
            });
        }
        StreamChunk tc_chunk;
        tc_chunk.tool_calls = final_calls;
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

std::pair<bool, std::string> OpenAICompatibleProvider::check_health() {
    auto res = utils::HttpClient::get(base_url_ + "/models", build_headers(), 5.0);
    if (res.status_code == 200 || res.status_code == 401 || res.status_code == 403) {
        return {true, "Acessível (HTTP " + std::to_string(res.status_code) + ")"};
    }
    if (res.status_code == 0) {
        return {false, "Servidor inacessível em " + base_url_};
    }
    return {false, "HTTP " + std::to_string(res.status_code)};
}

std::vector<std::string> OpenAICompatibleProvider::list_available_models() {
    auto res = utils::HttpClient::get(base_url_ + "/models", build_headers(), 5.0);
    if (res.success) {
        try {
            json data = json::parse(res.body);
            if (data.contains("data") && data["data"].is_array()) {
                std::vector<std::string> models;
                for (const auto& item : data["data"]) {
                    if (item.contains("id") && item["id"].is_string()) {
                        models.push_back(item["id"].get<std::string>());
                    }
                }
                if (!models.empty()) return models;
            }
        } catch (...) {}
    }
    return {model_name_};
}

} // namespace llmcli::providers
