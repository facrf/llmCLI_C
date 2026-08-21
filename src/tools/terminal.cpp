#include "llmcli/tools/terminal.hpp"
#include "llmcli/config.hpp"
#include <sstream>
#include <cstdio>
#include <array>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <signal.h>

namespace llmcli::tools {

json RunCommandTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {{"type", "string"}, {"description", "Comando de terminal a ser executado."}}},
            {"timeout_seconds", {{"type", "integer"}, {"description", "Tempo limite em segundos (padrão: 60)."}, {"default", 60}}}
        }},
        {"required", json::array({"command"})}
    };
}

ToolResult RunCommandTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();

    std::string cmd = arguments.value("command", "");
    Config& cfg = get_config();
    int timeout = arguments.value("timeout_seconds", cfg.security.command_timeout_seconds);

    if (cmd.empty()) {
        res.success = false;
        res.output = "Argumento 'command' obrigatório.";
        return res;
    }

    // Safety checks
    if (cmd.find("rm -rf /") != std::string::npos || cmd.find("mkfs") != std::string::npos || cmd.find(":(){ :|:& };:") != std::string::npos) {
        res.success = false;
        res.output = "Comando bloqueado por segurança (risco crítico ao sistema).";
        return res;
    }

    int out_pipe[2];
    int err_pipe[2];

    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        res.success = false;
        res.output = "Falha ao criar pipes IPC.";
        return res;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        res.success = false;
        res.output = "Falha ao criar subprocesso.";
        return res;
    }

    if (pid == 0) {
        // Child
        close(out_pipe[0]);
        close(err_pipe[0]);

        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(out_pipe[1]);
        close(err_pipe[1]);

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }

        if (chdir(cfg.project_root.c_str()) != 0) {
            _exit(127);
        }

        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(out_pipe[1]);
    close(err_pipe[1]);

    std::string stdout_str;
    std::string stderr_str;

    auto start_time = std::chrono::steady_clock::now();
    bool timed_out = false;

    // Read with timeout
    while (true) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            // Process finished, read remaining output
            std::array<char, 2048> buf;
            ssize_t n;
            while ((n = read(out_pipe[0], buf.data(), buf.size())) > 0) {
                stdout_str.append(buf.data(), n);
            }
            while ((n = read(err_pipe[0], buf.data(), buf.size())) > 0) {
                stderr_str.append(buf.data(), n);
            }

            close(out_pipe[0]);
            close(err_pipe[0]);

            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            res.success = (exit_code == 0);
            res.metadata["exit_code"] = exit_code;

            std::ostringstream out;
            out << "Código de saída: " << exit_code << "\n";
            if (!stdout_str.empty()) out << "[STDOUT]\n" << stdout_str << "\n";
            if (!stderr_str.empty()) out << "[STDERR]\n" << stderr_str << "\n";
            if (stdout_str.empty() && stderr_str.empty()) out << "(sem saída de texto)";

            res.output = out.str();
            return res;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
        if (elapsed > timeout) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            break;
        }

        // Read available data non-blocking or small sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    close(out_pipe[0]);
    close(err_pipe[0]);

    if (timed_out) {
        res.success = false;
        res.output = "Comando abortado: excedeu o tempo limite de " + std::to_string(timeout) + "s.";
        return res;
    }

    res.success = false;
    res.output = "Erro desconhecido na execução do comando.";
    return res;
}

} // namespace llmcli::tools
