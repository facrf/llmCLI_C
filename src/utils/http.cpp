#include "llmcli/utils/http.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdio>
#include <array>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>

namespace llmcli::utils {

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
    std::string cmd = "curl -s -S -w \"\\n%{http_code}\" --max-time " + std::to_string(static_cast<int>(timeout_seconds));
    
    for (const auto& [k, v] : headers) {
        cmd += " -H \"" + k + ": " + v + "\"";
    }
    cmd += " \"" + url + "\" 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        resp.error = "Falha ao abrir pipe curl";
        return resp;
    }

    std::string raw_output;
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        raw_output += buffer.data();
    }
    pclose(pipe);

    // Extract http_code from last line
    auto last_newline = raw_output.find_last_of("\r\n");
    std::string code_str;
    if (last_newline != std::string::npos) {
        auto prev_newline = raw_output.find_last_of("\r\n", last_newline - 1);
        if (prev_newline != std::string::npos) {
            code_str = raw_output.substr(prev_newline + 1, last_newline - prev_newline - 1);
            resp.body = raw_output.substr(0, prev_newline);
        } else {
            code_str = raw_output.substr(0, last_newline);
            resp.body = "";
        }
    } else {
        code_str = raw_output;
    }

    try {
        resp.status_code = std::stoi(code_str);
        resp.success = (resp.status_code >= 200 && resp.status_code < 300);
        if (resp.status_code == 0 && resp.error.empty()) {
            resp.error = "Servidor inacessível";
        }
    } catch (...) {
        resp.status_code = 0;
        resp.error = "Falha na requisição HTTP";
    }

    return resp;
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
        args.push_back("\n%{http_code}");
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

    // Extract http_code from last line
    auto last_newline = raw_output.find_last_of("\r\n");
    std::string code_str;
    if (last_newline != std::string::npos) {
        auto prev_newline = raw_output.find_last_of("\r\n", last_newline - 1);
        if (prev_newline != std::string::npos) {
            code_str = raw_output.substr(prev_newline + 1, last_newline - prev_newline - 1);
            resp.body = raw_output.substr(0, prev_newline);
        } else {
            code_str = raw_output.substr(0, last_newline);
            resp.body = "";
        }
    } else {
        code_str = raw_output;
    }

    try {
        resp.status_code = std::stoi(code_str);
        resp.success = (resp.status_code >= 200 && resp.status_code < 300);
    } catch (...) {
        resp.status_code = 0;
        resp.error = raw_output;
    }

    return resp;
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
            "curl", "-s", "-S", "-N", "-X", "POST",
            "--max-time", std::to_string(static_cast<int>(timeout_seconds)),
            "-H", "Content-Type: application/json"
        };

        for (const auto& [k, v] : headers) {
            args.push_back("-H");
            args.push_back(k + ": " + v);
        }

        args.push_back("--data-binary");
        args.push_back("@-");
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

    // Send payload
    size_t written = 0;
    while (written < payload_str.size()) {
        ssize_t n = write(in_pipe[1], payload_str.data() + written, payload_str.size() - written);
        if (n <= 0) break;
        written += n;
    }
    close(in_pipe[1]);

    // Read lines in real-time
    std::string accumulator;
    std::array<char, 2048> buf;
    ssize_t bytes_read = 0;

    while ((bytes_read = read(out_pipe[0], buf.data(), buf.size())) > 0) {
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

    if (!accumulator.empty()) {
        if (accumulator.back() == '\r') accumulator.pop_back();
        on_line(accumulator);
    }

    close(out_pipe[0]);

    int status;
    waitpid(pid, &status, 0);

    return true;
}

} // namespace llmcli::utils
