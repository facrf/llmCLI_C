#include "llmcli/providers/anthropic.hpp"
#include "llmcli/utils/http.hpp"
#include "llmcli/utils/env.hpp"
#include <iostream>

namespace llmcli::providers {

AnthropicProvider::AnthropicProvider(const std::string& model_name, const std::string& api_key)
    : LLMProvider(model_name) {
    if (model_name_.rfind("anthropic/", 0) == 0) {
        model_name_ = model_name_.substr(10);
    }
    api_key_ = api_key.empty() ? utils::EnvLoader::get_env("ANTHROPIC_API_KEY", "") : api_key;
}

std::pair<std::string, json> AnthropicProvider::convert_messages(const std::vector<ChatMessage>& messages) const {
    std::string system_prompt;
    json converted = json::array();

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            system_prompt += msg.content + "\n";
        } else if (msg.role == "tool") {
            json tool_block = {
                {"type", "tool_result"},
                {"tool_use_id", msg.tool_call_id.empty() ? "tool_call_id" : msg.tool_call_id},
                {"content", msg.content}
            };
            if (!converted.empty() && converted.back()["role"] == "user") {
                if (converted.back()["content"].is_array()) {
                    converted.back()["content"].push_back(tool_block);
                } else {
                    json arr = json::array({{{"type", "text"}, {"text", converted.back()["content"].get<std::string>()}}});
                    arr.push_back(tool_block);
                    converted.back()["content"] = arr;
                }
            } else {
                converted.push_back({
                    {"role", "user"},
                    {"content", json::array({tool_block})}
                });
            }
        } else if (msg.role == "assistant") {
            json content_blocks = json::array();
            if (!msg.content.empty()) {
                content_blocks.push_back({{"type", "text"}, {"text", msg.content}});
            }
            if (!msg.tool_calls.empty()) {
                for (const auto& tc : msg.tool_calls) {
                    content_blocks.push_back({
                        {"type", "tool_use"},
                        {"id", tc.id},
                        {"name", tc.name},
                        {"input", tc.arguments.is_object() ? tc.arguments : json::object()}
                    });
                }
            }
            if (content_blocks.empty()) {
                content_blocks.push_back({{"type", "text"}, {"text", ""}});
            }
            converted.push_back({
                {"role", "assistant"},
                {"content", content_blocks}
            });
        } else {
            // User
            if (!converted.empty() && converted.back()["role"] == "user") {
                if (converted.back()["content"].is_string()) {
                    converted.back()["content"] = converted.back()["content"].get<std::string>() + "\n" + msg.content;
                } else if (converted.back()["content"].is_array()) {
                    converted.back()["content"].push_back({{"type", "text"}, {"text", msg.content}});
                }
            } else {
                converted.push_back({
                    {"role", "user"},
                    {"content", msg.content}
                });
            }
        }
    }

    return {system_prompt, converted};
}

json AnthropicProvider::convert_tools(const std::vector<ToolDefinition>& tools) const {
    json converted = json::array();
    for (const auto& t : tools) {
        converted.push_back({
            {"name", t.name},
            {"description", t.description},
            {"input_schema", t.parameters}
        });
    }
    return converted;
}

bool AnthropicProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::vector<ToolDefinition>& tools,
    std::function<void(const StreamChunk& chunk)> on_chunk,
    double temperature,
    int max_tokens
) {
    if (api_key_.empty()) {
        StreamChunk err;
        err.is_done = true;
        err.error = "Chave ANTHROPIC_API_KEY não configurada no arquivo .env.";
        on_chunk(err);
        return false;
    }

    std::string url = "https://api.anthropic.com/v1/messages";
    auto [system_prompt, formatted_messages] = convert_messages(messages);

    json payload = {
        {"model", model_name_},
        {"messages", formatted_messages},
        {"max_tokens", max_tokens},
        {"temperature", temperature},
        {"stream", true}
    };

    if (!system_prompt.empty()) {
        payload["system"] = system_prompt;
    }

    if (!tools.empty()) {
        payload["tools"] = convert_tools(tools);
    }

    std::map<std::string, std::string> headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"}
    };

    struct CurrentToolState {
        std::string id;
        std::string name;
        std::string input_json;
    };
    std::unique_ptr<CurrentToolState> current_tool;
    std::vector<ToolCall> tool_calls;
    std::string out_err;
    std::optional<int> last_prompt_tokens;
    std::optional<int> last_completion_tokens;

    bool success = utils::HttpClient::post_stream(
        url,
        payload,
        headers,
        [&](const std::string& line) {
            std::string l = line;
            auto f = l.find_first_not_of(" \t\r\n");
            if (f == std::string::npos) return;
            l = l.substr(f);

            if (l.rfind("data: ", 0) == 0) {
                std::string raw_json = l.substr(6);
                try {
                    json ev = json::parse(raw_json);
                    std::string ev_type = ev.value("type", "");

                    if (ev_type == "message_start") {
                        if (ev.contains("message") && ev["message"].contains("usage")) {
                            auto u = ev["message"]["usage"];
                            if (u.contains("input_tokens") && u["input_tokens"].is_number()) {
                                last_prompt_tokens = u["input_tokens"].get<int>();
                            }
                        }
                    } else if (ev_type == "message_delta") {
                        if (ev.contains("usage")) {
                            auto u = ev["usage"];
                            if (u.contains("output_tokens") && u["output_tokens"].is_number()) {
                                last_completion_tokens = u["output_tokens"].get<int>();
                            }
                        }
                    } else if (ev_type == "content_block_start") {
                        if (ev.contains("content_block") && ev["content_block"].is_object()) {
                            auto cb = ev["content_block"];
                            if (cb.value("type", "") == "tool_use") {
                                current_tool = std::make_unique<CurrentToolState>();
                                current_tool->id = cb.value("id", "");
                                current_tool->name = cb.value("name", "");
                            }
                        }
                    } else if (ev_type == "content_block_delta") {
                        if (ev.contains("delta") && ev["delta"].is_object()) {
                            auto delta = ev["delta"];
                            std::string delta_type = delta.value("type", "");
                            if (delta_type == "text_delta") {
                                StreamChunk ch;
                                ch.delta_content = delta.value("text", "");
                                ch.prompt_tokens = last_prompt_tokens;
                                ch.completion_tokens = last_completion_tokens;
                                on_chunk(ch);
                            } else if (delta_type == "input_json_delta" && current_tool) {
                                current_tool->input_json += delta.value("partial_json", "");
                            }
                        }
                    } else if (ev_type == "content_block_stop") {
                        if (current_tool) {
                            json args = json::object();
                            try {
                                if (!current_tool->input_json.empty()) {
                                    args = json::parse(current_tool->input_json);
                                }
                            } catch (...) {}
                            tool_calls.push_back({current_tool->id, current_tool->name, args});
                            current_tool.reset();
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
        err.error = "Erro na conexão Anthropic: " + out_err;
        on_chunk(err);
        return false;
    }

    if (!tool_calls.empty()) {
        StreamChunk tc_chunk;
        tc_chunk.tool_calls = tool_calls;
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

std::pair<bool, std::string> AnthropicProvider::check_health() {
    if (api_key_.empty()) {
        return {false, "ANTHROPIC_API_KEY ausente no .env"};
    }
    return {true, "API Anthropic configurada"};
}

} // namespace llmcli::providers
