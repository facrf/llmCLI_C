#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace llmcli::utils {

using json = nlohmann::json;

struct HttpResponse {
    int status_code{0};
    std::string body;
    std::string error;
    bool success{false};
};

class HttpClient {
public:
    // Synchronous GET request
    static HttpResponse get(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {},
        double timeout_seconds = 10.0
    );

    // Synchronous POST request with JSON payload
    static HttpResponse post_json(
        const std::string& url,
        const json& payload,
        const std::map<std::string, std::string>& headers = {},
        double timeout_seconds = 60.0
    );

    // Streaming POST request with SSE / line-by-line callback
    // on_line callback receives each SSE/data line in real-time
    static bool post_stream(
        const std::string& url,
        const json& payload,
        const std::map<std::string, std::string>& headers,
        std::function<void(const std::string& line)> on_line,
        std::string& out_error,
        double timeout_seconds = 120.0
    );

    // URL encoder
    static std::string url_encode(const std::string& value);

    // Stream cancellation control
    static void cancel_active_stream();
    static bool is_stream_cancelled();
    static void reset_stream_cancel_flag();
};

} // namespace llmcli::utils
