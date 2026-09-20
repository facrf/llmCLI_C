#include "llmcli/tools/terminal.hpp"
#include "llmcli/config.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace llmcli::tools {
namespace {
constexpr size_t kMaxCapturedOutput = 1024 * 1024;

bool has_disallowed_shell_syntax(const std::string& command) {
    // This tool accepts one command with arguments. Shell composition can escape
    // the workspace through redirects, substitutions, and chained commands.
    return command.find_first_of(";|&<>`\n\r$\"'") != std::string::npos ||
           command.find("../") != std::string::npos ||
           command.find('~') != std::string::npos;
}

bool has_blocked_program(const std::string& command) {
    std::istringstream input(command);
    std::string program;
    input >> program;
    const bool destructive_program = program == "rm" || program == "rmdir" || program == "unlink" ||
        program == "mkfs" || program == "dd" || program == "shutdown" || program == "reboot" ||
        program == "poweroff" || program == "halt" || program == "kill" || program == "pkill" ||
        program == "killall" || program == "chown" || program == "chmod";
    return destructive_program || command.rfind("git reset", 0) == 0 ||
        command.rfind("git restore", 0) == 0 || command.rfind("git clean", 0) == 0 ||
        command.rfind("git checkout", 0) == 0 || command.rfind("git switch", 0) == 0;
}

void append_limited(std::string& dest, const char* data, size_t size, bool& truncated) {
    const size_t available = dest.size() < kMaxCapturedOutput ? kMaxCapturedOutput - dest.size() : 0;
    const size_t accepted = std::min(size, available);
    dest.append(data, accepted);
    truncated = truncated || accepted != size;
}
} // namespace

json RunCommandTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"command", {{"type", "string"}, {"description", "Comando simples a ser executado."}}},
        {"timeout_seconds", {{"type", "integer"}, {"description", "Tempo limite em segundos (padrão: 60)."}, {"default", 60}}}
    }}, {"required", json::array({"command"})}};
}

ToolResult RunCommandTool::execute(const json& arguments) {
    ToolResult res;
    res.name = name();
    const std::string cmd = arguments.value("command", "");
    Config& cfg = get_config();
    const int timeout = std::clamp(arguments.value("timeout_seconds", cfg.security.command_timeout_seconds), 1, 3600);
    if (cmd.empty()) {
        res.success = false;
        res.output = "Argumento 'command' obrigatório.";
        return res;
    }
    if (has_disallowed_shell_syntax(cmd) || has_blocked_program(cmd)) {
        res.success = false;
        res.output = "Comando bloqueado: pipes, redirecionamentos, expansões, caminhos ascendentes e comandos compostos não são permitidos.";
        return res;
    }

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        res.success = false;
        res.output = "Falha ao criar pipes IPC.";
        return res;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
        res.success = false;
        res.output = "Falha ao criar subprocesso.";
        return res;
    }
    if (pid == 0) {
        setpgid(0, 0);
        close(out_pipe[0]); close(err_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO); dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[1]); close(err_pipe[1]);
        const int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        if (chdir(cfg.project_root.c_str()) != 0) _exit(127);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }

    close(out_pipe[1]); close(err_pipe[1]);
    fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, fcntl(err_pipe[0], F_GETFL) | O_NONBLOCK);
    std::string stdout_str, stderr_str;
    bool timed_out = false, truncated = false, child_finished = false;
    int status = 0;
    const auto start = std::chrono::steady_clock::now();

    // Continuously drain both pipes: waiting before reading can deadlock once a
    // child fills a kernel pipe buffer.
    while (!child_finished) {
        pollfd fds[] = {{out_pipe[0], POLLIN | POLLHUP, 0}, {err_pipe[0], POLLIN | POLLHUP, 0}};
        poll(fds, 2, 50);
        std::array<char, 4096> buffer;
        for (auto& fd : fds) {
            while (fd.revents & (POLLIN | POLLHUP)) {
                const ssize_t count = read(fd.fd, buffer.data(), buffer.size());
                if (count <= 0) break;
                append_limited(fd.fd == out_pipe[0] ? stdout_str : stderr_str, buffer.data(), static_cast<size_t>(count), truncated);
            }
        }
        child_finished = waitpid(pid, &status, WNOHANG) == pid;
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
        if (!child_finished && elapsed > timeout) {
            timed_out = true;
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            child_finished = true;
        }
    }
    close(out_pipe[0]); close(err_pipe[0]);
    if (timed_out) {
        res.success = false;
        res.output = "Comando abortado: excedeu o tempo limite de " + std::to_string(timeout) + "s.";
        return res;
    }

    const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    res.success = exit_code == 0;
    res.metadata["exit_code"] = exit_code;
    std::ostringstream out;
    out << "Código de saída: " << exit_code << "\n";
    if (!stdout_str.empty()) out << "[STDOUT]\n" << stdout_str << "\n";
    if (!stderr_str.empty()) out << "[STDERR]\n" << stderr_str << "\n";
    if (stdout_str.empty() && stderr_str.empty()) out << "(sem saída de texto)";
    if (truncated) out << "\n[Saída truncada em 1 MiB]";
    res.output = out.str();
    return res;
}
} // namespace llmcli::tools
