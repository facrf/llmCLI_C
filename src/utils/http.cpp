#include "llmcli/utils/http.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdio>
#include <array>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <atomic>
#include <csignal>

namespace llmcli::utils {

static std::atomic<bool> g_stream_cancelled{false};
static std::atomic<pid_t> g_active_curl_pid{0};
static constexpr const char* HTTP_STATUS_MARKER = "\n__LLMCLI_HTTP_STATUS__:";

HttpResponse HttpClient::parse_curl_response(const std::string& raw_output, int exit_code) {
    HttpResponse resp;
    const std::string marker = HTTP_STATUS_MARKER;
    const auto marker_pos = raw_output.rfind(marker);
    if (marker_pos == std::string::npos) {
        resp.error = raw_output.empty() ? "Falha na requisição HTTP" : raw_output;
        return resp;
    }

    resp.body = raw_output.substr(0, marker_pos);
    std::string code_str = raw_output.substr(marker_pos + marker.size());
    const auto first = code_str.find_first_not_of(" \t\r\n");
    const auto last = code_str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos) {
        resp.error = "Resposta HTTP sem código de status";
        return resp;
    }
    code_str = code_str.substr(first, last - first + 1);

    try {
        resp.status_code = std::stoi(code_str);
    } catch (...) {
        resp.error = "Código de status HTTP inválido: " + code_str;
        return resp;
    }

    resp.success = exit_code == 0 && resp.status_code >= 200 && resp.status_code < 300;
    if (exit_code != 0) {
        resp.error = resp.body.empty()
            ? "curl encerrou com código " + std::to_string(exit_code)
            : resp.body;
    } else if (resp.status_code == 0) {
        resp.error = "Servidor inacessível";
    }
    return resp;
}

void HttpClient::cancel_active_stream() {
    g_stream_cancelled = true;
    pid_t p = g_active_curl_pid.load();
    if (p > 0) {
        kill(p, SIGTERM);
    }
}

bool HttpClient::is_stream_cancelled() {
    return g_stream_cancelled.load();
}

void HttpClient::reset_stream_cancel_flag() {
    g_stream_cancelled = false;
    g_active_curl_pid = 0;
}

std::string HttpClient::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

HttpResponse HttpClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    double timeout_seconds
) {
    HttpResponse resp;
    int out_pipe[2];
    if (pipe(out_pipe) != 0) {
        resp.error = "Falha ao criar pipe para curl";
        return resp;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        resp.error = "Falha no fork do processo curl";
        return resp;
    }
    if (pid == 0) {
        close(out_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(out_pipe[1]);
        std::vector<std::string> args = {
            "curl", "-s", "-S", "--max-time",
            std::to_string(std::max(1, static_cast<int>(timeout_seconds))),
            "-w", std::string(HTTP_STATUS_MARKER) + "%{http_code}"
        };
        for (const auto& [key, value] : headers) {
            args.push_back("-H");
            args.push_back(key + ": " + value);
        }
        args.push_back("--");
        args.push_back(url);
        std::vector<char*> c_args;
        for (auto& arg : args) c_args.push_back(arg.data());
        c_args.push_back(nullptr);
        execvp("curl", c_args.data());
        _exit(127);
    }
    close(out_pipe[1]);

    std::string raw_output;
    std::array<char, 4096> buffer;
    ssize_t bytes_read;
    while ((bytes_read = read(out_pipe[0], buffer.data(), buffer.size())) > 0) {
        raw_output.append(buffer.data(), bytes_read);
    }
    close(out_pipe[0]);
    int process_status = 0;
    waitpid(pid, &process_status, 0);

    const int exit_code = WIFEXITED(process_status) ? WEXITSTATUS(process_status) : -1;
    return parse_curl_response(raw_output, exit_code);
}

HttpResponse HttpClient::post_json(
    const std::string& url,
    const json& payload,
    const std::map<std::string, std::string>& headers,
    double timeout_seconds
) {
    HttpResponse resp;
    std::string payload_str = payload.dump();

    // Create pipes for communicating with curl
    int in_pipe[2];  // C++ writes to in_pipe[1], curl reads from in_pipe[0]
    int out_pipe[2]; // curl writes to out_pipe[1], C++ reads from out_pipe[0]

    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        resp.error = "Falha ao criar pipes IPC";
        return resp;
    }

    pid_t pid = fork();
    if (pid < 0) {
        resp.error = "Falha no fork do processo curl";
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return resp;
    }

    if (pid == 0) {
        // Child process: execute curl
        close(in_pipe[1]);
        close(out_pipe[0]);

        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);

        close(in_pipe[0]);
        close(out_pipe[1]);

        std::vector<std::string> args = {
            "curl", "-s", "-S", "-X", "POST",
            "--max-time", std::to_string(static_cast<int>(timeout_seconds)),
            "-H", "Content-Type: application/json"
        };

        for (const auto& [k, v] : headers) {
            args.push_back("-H");
            args.push_back(k + ": " + v);
        }

        args.push_back("--data-binary");
        args.push_back("@-");
        args.push_back("-w");
        args.push_back(std::string(HTTP_STATUS_MARKER) + "%{http_code}");
        args.push_back("--");
        args.push_back(url);

        std::vector<char*> c_args;
        for (auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp("curl", c_args.data());
        _exit(127);
    }

    // Parent process
    close(in_pipe[0]);
    close(out_pipe[1]);

    // Write payload to curl stdin
    size_t written = 0;
    while (written < payload_str.size()) {
        ssize_t n = write(in_pipe[1], payload_str.data() + written, payload_str.size() - written);
        if (n <= 0) break;
        written += n;
    }
    close(in_pipe[1]);

    // Read response from curl stdout
    std::string raw_output;
    std::array<char, 4096> buf;
    ssize_t bytes_read = 0;
    while ((bytes_read = read(out_pipe[0], buf.data(), buf.size())) > 0) {
        raw_output.append(buf.data(), bytes_read);
    }
    close(out_pipe[0]);

    int status;
    waitpid(pid, &status, 0);

    const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return parse_curl_response(raw_output, exit_code);
}

bool HttpClient::post_stream(
    const std::string& url,
    const json& payload,
    const std::map<std::string, std::string>& headers,
    std::function<void(const std::string& line)> on_line,
    std::string& out_error,
    double timeout_seconds
) {
    std::string payload_str = payload.dump();

    int in_pipe[2];
    int out_pipe[2];

    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        out_error = "Falha ao criar pipes para streaming";
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        out_error = "Falha no fork do processo curl";
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return false;
    }

    if (pid == 0) {
        // Child: curl -N (no-buffer)
        close(in_pipe[1]);
        close(out_pipe[0]);

        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);

        close(in_pipe[0]);
        close(out_pipe[1]);

        std::vector<std::string> args = {
            "curl", "-s", "-S", "-N", "--fail-with-body", "-X", "POST",
            "--max-time", std::to_string(static_cast<int>(timeout_seconds)),
            "-H", "Content-Type: application/json"
        };

        for (const auto& [k, v] : headers) {
            args.push_back("-H");
            args.push_back(k + ": " + v);
        }

        args.push_back("--data-binary");
        args.push_back("@-");
        args.push_back("--");
        args.push_back(url);

        std::vector<char*> c_args;
        for (auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp("curl", c_args.data());
        _exit(127);
    }

    // Parent
    close(in_pipe[0]);
    close(out_pipe[1]);

    g_stream_cancelled = false;
    g_active_curl_pid = pid;

    // Send payload
    size_t written = 0;
    while (written < payload_str.size() && !g_stream_cancelled) {
        ssize_t n = write(in_pipe[1], payload_str.data() + written, payload_str.size() - written);
        if (n <= 0) break;
        written += n;
    }
    close(in_pipe[1]);

    // Read lines in real-time
    std::string accumulator;
    std::array<char, 2048> buf;
    ssize_t bytes_read = 0;

    while (!g_stream_cancelled && (bytes_read = read(out_pipe[0], buf.data(), buf.size())) > 0) {
        accumulator.append(buf.data(), bytes_read);

        size_t pos = 0;
        while ((pos = accumulator.find('\n')) != std::string::npos) {
            std::string line = accumulator.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            on_line(line);
            accumulator.erase(0, pos + 1);
        }
    }

    if (!g_stream_cancelled && !accumulator.empty()) {
        if (accumulator.back() == '\r') accumulator.pop_back();
        on_line(accumulator);
    }

    close(out_pipe[0]);
    g_active_curl_pid = 0;

    if (g_stream_cancelled) {
        kill(pid, SIGTERM);
        out_error = "Operação cancelada pelo usuário.";
    }

    int status;
    waitpid(pid, &status, 0);

    const bool process_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!g_stream_cancelled && !process_ok && out_error.empty()) {
        out_error = "curl encerrou com código " +
            std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    return !g_stream_cancelled && process_ok;
}

} // namespace llmcli::utils
