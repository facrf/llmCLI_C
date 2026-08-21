#include "llmcli/tools/git_ops.hpp"
#include "llmcli/config.hpp"
#include <sstream>
#include <cstdio>
#include <array>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace llmcli::tools {

std::tuple<int, std::string, std::string> run_git_cmd(const std::vector<std::string>& args) {
    Config& cfg = get_config();

    int out_pipe[2];
    int err_pipe[2];

    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        return {-1, "", "Falha ao criar pipes para comando git"};
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return {-1, "", "Falha no fork para comando git"};
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

        std::vector<std::string> full_args = {"git"};
        full_args.insert(full_args.end(), args.begin(), args.end());

        std::vector<char*> c_args;
        for (auto& a : full_args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp("git", c_args.data());
        _exit(127);
    }

    // Parent
    close(out_pipe[1]);
    close(err_pipe[1]);

    std::string stdout_str;
    std::string stderr_str;
    std::array<char, 2048> buf;
    ssize_t bytes_read;

    while ((bytes_read = read(out_pipe[0], buf.data(), buf.size())) > 0) {
        stdout_str.append(buf.data(), bytes_read);
    }
    close(out_pipe[0]);

    while ((bytes_read = read(err_pipe[0], buf.data(), buf.size())) > 0) {
        stderr_str.append(buf.data(), bytes_read);
    }
    close(err_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    // Trim trailing newline
    if (!stdout_str.empty() && stdout_str.back() == '\n') stdout_str.pop_back();
    if (!stderr_str.empty() && stderr_str.back() == '\n') stderr_str.pop_back();

    return {exit_code, stdout_str, stderr_str};
}

bool is_git_repo() {
    auto [code, out, err] = run_git_cmd({"rev-parse", "--is-inside-work-tree"});
    return (code == 0);
}

std::string get_git_diff(bool cached) {
    if (!is_git_repo()) {
        return "(Repositório Git não inicializado neste diretório)";
    }
    std::vector<std::string> args = {"diff"};
    if (cached) args.push_back("--cached");

    auto [code, out, err] = run_git_cmd(args);
    if (code != 0) return "Erro ao obter diff: " + err;
    return out.empty() ? "(Nenhuma modificação não commitada no momento)" : out;
}

std::string get_raw_git_diff() {
    if (!is_git_repo()) return "";
    auto [c1, out1, e1] = run_git_cmd({"diff"});
    auto [c2, out2, e2] = run_git_cmd({"diff", "--cached"});

    std::string combined;
    if (!out1.empty()) combined += out1;
    if (!out2.empty()) {
        if (!combined.empty()) combined += "\n\n";
        combined += out2;
    }
    return combined;
}

std::string get_git_status() {
    if (!is_git_repo()) return "(Repositório Git não inicializado)";
    auto [code, out, err] = run_git_cmd({"status", "--short"});
    if (code != 0) return "Erro ao obter status: " + err;
    return out.empty() ? "(Árvore de trabalho limpa)" : out;
}

std::optional<std::string> create_checkpoint_commit(const std::string& message) {
    Config& cfg = get_config();
    if (!cfg.git.auto_commit_on_edit || !is_git_repo()) return std::nullopt;

    run_git_cmd({"add", "-A"});
    std::string full_msg = cfg.git.commit_prefix + " " + message;
    auto [code, out, err] = run_git_cmd({"commit", "-m", full_msg});
    if (code == 0) {
        auto [c2, hash_out, e2] = run_git_cmd({"rev-parse", "--short", "HEAD"});
        if (c2 == 0) return hash_out;
    }
    return std::nullopt;
}

std::pair<bool, std::string> undo_last_checkpoint() {
    if (!is_git_repo()) {
        return {false, "Git não está configurado neste diretório."};
    }

    Config& cfg = get_config();
    auto [code, last_msg, err] = run_git_cmd({"log", "-1", "--pretty=%B"});
    if (code == 0 && last_msg.find(cfg.git.commit_prefix) != std::string::npos) {
        auto [code_reset, _, err_reset] = run_git_cmd({"reset", "--hard", "HEAD~1"});
        if (code_reset == 0) {
            return {true, "Última alteração desfeita com sucesso (Commit revertido: " + last_msg + ")."};
        }
        return {false, "Falha ao reverter commit: " + err_reset};
    }

    // Try restoring working tree
    auto [code_rest, _, err_rest] = run_git_cmd({"restore", "."});
    if (code_rest == 0) {
        return {true, "Modificações não commitadas foram revertidas com sucesso."};
    }
    return {false, "Não foi possível reverter: " + err_rest};
}

std::pair<bool, std::string> create_user_commit(const std::string& message) {
    if (!is_git_repo()) {
        return {false, "Git não está configurado neste repositório."};
    }

    run_git_cmd({"add", "-A"});
    auto [code, out, err] = run_git_cmd({"commit", "-m", message});
    if (code == 0) {
        auto [c2, hash_out, e2] = run_git_cmd({"rev-parse", "--short", "HEAD"});
        return {true, "Commit criado com sucesso [" + hash_out + "]: " + message};
    }
    return {false, "Erro ao criar commit: " + (err.empty() ? out : err)};
}

} // namespace llmcli::tools
