#include "llmcli/core/exporter.hpp"
#include "llmcli/config.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace llmcli::core {

SessionExporter::SessionExporter(const Session& session, const std::filesystem::path& project_root)
    : session_(session) {
    if (project_root.empty()) {
        project_root_ = get_config().project_root;
    } else {
        project_root_ = project_root;
    }
}

std::string SessionExporter::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}

std::string SessionExporter::escape_html(const std::string& text) {
    std::ostringstream out;
    for (char c : text) {
        switch (c) {
            case '&':  out << "&amp;"; break;
            case '<':  out << "&lt;"; break;
            case '>':  out << "&gt;"; break;
            case '"':  out << "&quot;"; break;
            case '\'': out << "&#39;"; break;
            case '\n': out << "<br>"; break;
            default:   out << c; break;
        }
    }
    return out.str();
}

std::filesystem::path SessionExporter::export_markdown(const std::filesystem::path& target_path) const {
    Config& cfg = get_config();
    std::filesystem::path dest;
    if (target_path.empty()) {
        dest = project_root_ / ("session_export_" + current_timestamp() + ".md");
    } else {
        dest = target_path.is_absolute() ? target_path : (project_root_ / target_path);
    }

    auto [p_tok, c_tok, tot_tok] = session_.get_cumulative_tokens();
    auto tracked = session_.file_tracker().list_files();

    std::ostringstream out;
    out << "# 📄 Relatório de Sessão llmCli\n"
        << "- **Data/Hora:** " << current_timestamp() << "\n"
        << "- **Modelo Ativo:** `" << cfg.active_model << "`\n"
        << "- **Modo YOLO:** `" << (cfg.yolo_mode ? "Ativado" : "Desativado") << "`\n"
        << "- **Modo Arquiteto:** `" << (cfg.architect_mode ? "Ativado (" + cfg.architect_model + ")" : "Desativado") << "`\n"
        << "- **Tokens Acumulados:** ~" << tot_tok << " tokens (~" << p_tok << " prompt + ~" << c_tok << " completion)\n"
        << "- **Arquivos no Contexto (" << tracked.size() << "):** ";

    if (tracked.empty()) {
        out << "Nenhum\n";
    } else {
        for (size_t i = 0; i < tracked.size(); ++i) {
            out << "`" << tracked[i] << "`" << (i + 1 < tracked.size() ? ", " : "\n");
        }
    }

    out << "\n---\n\n## 💬 Histórico da Conversa\n\n";

    for (const auto& msg : session_.messages()) {
        if (msg.role == "user") {
            out << "### 👤 Usuário\n\n" << msg.content << "\n\n";
        } else if (msg.role == "assistant") {
            out << "### 🤖 Assistente (" << cfg.active_model << ")\n\n" << msg.content << "\n\n";
        } else if (msg.role == "tool") {
            out << "#### 🛠️ Ferramenta: `" << (msg.name.empty() ? "tool" : msg.name) << "`\n\n```text\n"
                << msg.content << "\n```\n\n";
        } else {
            out << "#### ⚙️ Sistema\n\n" << msg.content << "\n\n";
        }
    }

    if (dest.has_parent_path()) {
        std::filesystem::create_directories(dest.parent_path());
    }
    std::ofstream f(dest);
    f << out.str();

    return dest;
}

std::filesystem::path SessionExporter::export_html(const std::filesystem::path& target_path) const {
    Config& cfg = get_config();
    std::filesystem::path dest;
    if (target_path.empty()) {
        dest = project_root_ / ("session_export_" + current_timestamp() + ".html");
    } else {
        dest = target_path.is_absolute() ? target_path : (project_root_ / target_path);
    }

    auto [p_tok, c_tok, tot_tok] = session_.get_cumulative_tokens();
    auto tracked = session_.file_tracker().list_files();

    std::ostringstream msgs_html;
    for (const auto& msg : session_.messages()) {
        std::string badge, cls;
        if (msg.role == "user") {
            badge = "<span class=\"badge user\">👤 Usuário</span>";
            cls = "msg-user";
        } else if (msg.role == "assistant") {
            badge = "<span class=\"badge assistant\">🤖 Assistente (" + cfg.active_model + ")</span>";
            cls = "msg-assistant";
        } else if (msg.role == "tool") {
            badge = "<span class=\"badge tool\">🛠️ Ferramenta: " + (msg.name.empty() ? "tool" : msg.name) + "</span>";
            cls = "msg-tool";
        } else {
            badge = "<span class=\"badge system\">⚙️ Sistema</span>";
            cls = "msg-system";
        }

        msgs_html << "<div class=\"message " << cls << "\">\n"
                  << "  <div class=\"msg-header\">" << badge << "</div>\n"
                  << "  <div class=\"msg-body\">" << escape_html(msg.content) << "</div>\n"
                  << "</div>\n";
    }

    std::string tracked_str = "Nenhum";
    if (!tracked.empty()) {
        std::ostringstream ss;
        for (size_t i = 0; i < tracked.size(); ++i) {
            ss << tracked[i] << (i + 1 < tracked.size() ? ", " : "");
        }
        tracked_str = ss.str();
    }

    std::ostringstream out;
    out << R"(<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <title>llmCli - Relatório de Sessão</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 2rem; line-height: 1.6; }
        .container { max-width: 900px; margin: 0 auto; }
        .header { background: #1e293b; padding: 1.5rem; border-radius: 8px; margin-bottom: 2rem; border-left: 4px solid #38bdf8; }
        .header h1 { margin: 0 0 1rem 0; color: #38bdf8; font-size: 1.8rem; }
        .meta-item { margin: 0.3rem 0; color: #94a3b8; }
        .meta-item strong { color: #e2e8f0; }
        .message { margin-bottom: 1.5rem; padding: 1rem 1.2rem; border-radius: 8px; }
        .msg-user { background: #1e293b; border-left: 4px solid #22c55e; }
        .msg-assistant { background: #1e293b; border-left: 4px solid #38bdf8; }
        .msg-tool { background: #1e293b; border-left: 4px solid #eab308; font-family: monospace; font-size: 0.9rem; }
        .msg-system { background: #1e293b; border-left: 4px solid #94a3b8; font-size: 0.9rem; }
        .msg-header { margin-bottom: 0.6rem; font-weight: bold; }
        .badge { padding: 0.2rem 0.6rem; border-radius: 4px; font-size: 0.85rem; }
        .badge.user { background: #15803d; color: #fff; }
        .badge.assistant { background: #0369a1; color: #fff; }
        .badge.tool { background: #854d0e; color: #fff; }
        .badge.system { background: #475569; color: #fff; }
        .msg-body { white-space: pre-wrap; word-break: break-word; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📄 llmCli - Relatório de Sessão</h1>
            <div class="meta-item"><strong>Data:</strong> )" << current_timestamp() << R"(</div>
            <div class="meta-item"><strong>Modelo Ativo:</strong> )" << cfg.active_model << R"(</div>
            <div class="meta-item"><strong>Tokens Acumulados:</strong> ~)" << tot_tok << " (~" << p_tok << " prompt + ~" << c_tok << R"( completion)</div>
            <div class="meta-item"><strong>Arquivos no Contexto:</strong> )" << escape_html(tracked_str) << R"(</div>
        </div>
        <div class="chat-flow">
)" << msgs_html.str() << R"(
        </div>
    </div>
</body>
</html>
)";

    if (dest.has_parent_path()) {
        std::filesystem::create_directories(dest.parent_path());
    }
    std::ofstream f(dest);
    f << out.str();

    return dest;
}

} // namespace llmcli::core
