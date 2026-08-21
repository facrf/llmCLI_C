#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace llmcli {

using json = nlohmann::json;

// Tool Call representation
struct ToolCall {
    std::string id;
    std::string name;
    json arguments; // parsed JSON object or map

    json to_json() const {
        return json{
            {"id", id},
            {"type", "function"},
            {"function", {
                {"name", name},
                {"arguments", arguments.is_string() ? arguments.get<std::string>() : arguments.dump()}
            }}
        };
    }
};

// Result of executing a tool
struct ToolResult {
    std::string tool_call_id;
    std::string name;
    bool success{false};
    std::string output;
    json metadata = json::object();

    std::string to_message_text() const {
        std::string status = success ? "SUCESSO" : "ERRO";
        return "[" + status + "] Ferramenta '" + name + "':\n" + output;
    }
};

// Schema / Definition of a Tool
struct ToolDefinition {
    std::string name;
    std::string description;
    json parameters;

    json to_json() const {
        return json{
            {"type", "function"},
            {"function", {
                {"name", name},
                {"description", description},
                {"parameters", parameters}
            }}
        };
    }
};

// Chat message representation
struct ChatMessage {
    std::string role; // "system", "user", "assistant", "tool"
    std::string content;
    std::vector<ToolCall> tool_calls;
    std::string tool_call_id;
    std::string name;

    json to_json() const {
        json j = {
            {"role", role},
            {"content", content}
        };
        if (!tool_calls.empty()) {
            json tcs = json::array();
            for (const auto& tc : tool_calls) {
                tcs.push_back(tc.to_json());
            }
            j["tool_calls"] = tcs;
        }
        if (!tool_call_id.empty()) {
            j["tool_call_id"] = tool_call_id;
        }
        if (!name.empty()) {
            j["name"] = name;
        }
        return j;
    }
};

// Streaming chunk
struct StreamChunk {
    std::string delta_content;
    std::vector<ToolCall> tool_calls;
    bool is_done{false};
    std::string finish_reason;
    std::string error;
    std::optional<int> prompt_tokens;
    std::optional<int> completion_tokens;
};

// Model Preset for CLI menu
struct ModelPreset {
    int id{0};
    std::string name;
    std::string category;
    std::string desc;
};

// Discovered Service from network scanner
struct DiscoveredService {
    std::string service_name;
    std::string base_url;
    std::string provider_type; // "ollama", "llamacpp", "lmstudio", "vllm", "openai_compatible"
    std::string status;
    std::string version;
    std::vector<std::string> models;
    std::string details;
};

// Code Chunk for Semantic Indexer
struct CodeChunk {
    std::string file_path;
    std::string symbol_name;
    std::string symbol_type; // "class", "function", "module", "block"
    int start_line{1};
    int end_line{1};
    std::string content;

    json to_json() const {
        return json{
            {"file_path", file_path},
            {"symbol_name", symbol_name},
            {"symbol_type", symbol_type},
            {"start_line", start_line},
            {"end_line", end_line},
            {"content", content}
        };
    }

    static CodeChunk from_json(const json& j) {
        CodeChunk c;
        c.file_path = j.value("file_path", "");
        c.symbol_name = j.value("symbol_name", "");
        c.symbol_type = j.value("symbol_type", "block");
        c.start_line = j.value("start_line", 1);
        c.end_line = j.value("end_line", 1);
        c.content = j.value("content", "");
        return c;
    }
};

struct SearchResult {
    CodeChunk chunk;
    double score{0.0};
};

// Search & Replace block for diff applier
struct SearchReplaceBlock {
    std::string file_path;
    std::string search_content;
    std::string replace_content;
};

// Todo Item for task planner
struct TodoItem {
    int id{0};
    std::string text;
    bool done{false};

    json to_json() const {
        return json{
            {"id", id},
            {"text", text},
            {"done", done}
        };
    }

    static TodoItem from_json(const json& j) {
        TodoItem item;
        item.id = j.value("id", 0);
        item.text = j.value("text", "");
        item.done = j.value("done", false);
        return item;
    }
};

} // namespace llmcli
