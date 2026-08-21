#include "llmcli/ui/console.hpp"
#include "llmcli/utils/ansi.hpp"
#include "llmcli/i18n.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace llmcli::ui {

void print_banner(const std::string& active_model, bool yolo_mode) {
    std::string yolo_badge = yolo_mode ? std::string(ansi::BOLD_RED) + "⚡ YOLO: ON" + ansi::RESET
                                       : std::string(ansi::BOLD_GREEN) + "🛡️ YOLO: OFF" + ansi::RESET;

    std::string title = std::string(ansi::BOLD_CYAN) + i18n::t("banner_subtitle") + ansi::RESET;
    std::string subtitle = "Modelo Ativo: " + std::string(ansi::BOLD_YELLOW) + active_model + ansi::RESET + " | Modo: " + yolo_badge;
    std::string desc = std::string(ansi::DIM) + i18n::t("banner_desc") + ansi::RESET;

    std::string body = title + "\n" + subtitle + "\n\n" + desc;
    ansi::print_panel(body, "", ansi::CYAN);
}

void print_diff(const std::string& diff_text, const std::string& filename) {
    if (diff_text.empty()) return;
    std::cout << ansi::format_diff(diff_text, filename) << std::endl;
}

void print_tool_execution(const std::string& tool_name, const std::string& args_summary, bool is_yolo) {
    std::string badge = is_yolo ? std::string(ansi::BOLD_RED) + "[YOLO AUTO]" + ansi::RESET
                                : std::string(ansi::BOLD_CYAN) + "[FERRAMENTA]" + ansi::RESET;
    std::cout << "\n" << badge << " Executando " << ansi::BOLD_YELLOW << tool_name << ansi::RESET
              << "(" << args_summary << ")...\n";
}

void print_tool_result(const std::string& tool_name, bool success, const std::string& output) {
    std::string style = success ? ansi::BOLD_GREEN : ansi::BOLD_RED;
    std::string icon = success ? "✓" : "✗";
    std::cout << style << icon << " " << tool_name << ":" << ansi::RESET << "\n";

    std::stringstream ss(output);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    if (lines.size() > 10) {
        std::ostringstream preview;
        for (size_t i = 0; i < 10; ++i) {
            preview << lines[i] << "\n";
        }
        preview << "... (+ " << (lines.size() - 10) << " linhas ocultas)";
        ansi::print_panel(preview.str(), "", ansi::DIM);
    } else {
        ansi::print_panel(output, "", ansi::DIM);
    }
}

std::string ask_user_confirmation(const std::string& prompt_text) {
    std::cout << "\n" << ansi::BOLD_YELLOW << "❓ " << prompt_text << ansi::RESET << "\n";
    std::cout << ansi::DIM << "([s]im / [N]ão / [y]olo para liberar tudo / [c]ancelar) " << ansi::RESET << std::flush;

    std::string input;
    if (!std::getline(std::cin, input)) {
        std::cin.clear();
        return "abort";
    }

    // Sanitize any raw backspaces (^H / ^?)
    std::string clean_input;
    for (char ch : input) {
        if (ch == 127 || ch == 8) {
            if (!clean_input.empty()) clean_input.pop_back();
        } else {
            clean_input.push_back(ch);
        }
    }
    input = clean_input;

    auto f = input.find_first_not_of(" \t\r\n");
    auto l = input.find_last_not_of(" \t\r\n");
    if (f == std::string::npos) return "no";
    input = input.substr(f, l - f + 1);
    std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c){ return std::tolower(c); });

    if (input == "s" || input == "sim" || input == "y" || input == "yes") {
        return "yes";
    }
    if (input == "yolo" || input == "yolo!") {
        return "yolo";
    }
    if (input == "c" || input == "cancel" || input == "abort") {
        return "abort";
    }
    return "no";
}

void print_status_table(const std::vector<providers::ProviderHealthItem>& status_list) {
    std::vector<std::string> headers = {"Provedor", "Endpoint / Origem", "Status", "Detalhes", "Exemplo de Uso"};
    std::vector<std::vector<std::string>> rows;

    for (const auto& item : status_list) {
        std::string status_styled;
        if (item.status == "ONLINE" || item.status == "CONFIGURADO") {
            status_styled = std::string(ansi::BOLD_GREEN) + item.status + ansi::RESET;
        } else if (item.status == "SEM CHAVE") {
            status_styled = std::string(ansi::BOLD_YELLOW) + item.status + ansi::RESET;
        } else {
            status_styled = std::string(ansi::BOLD_RED) + item.status + ansi::RESET;
        }

        std::string detail = item.detail;
        if (!item.models.empty()) {
            detail += " (" + std::to_string(item.models.size()) + " modelos)";
        }

        rows.push_back({
            item.provider,
            item.endpoint,
            status_styled,
            detail,
            std::string(ansi::YELLOW) + item.example + ansi::RESET
        });
    }

    ansi::print_table(headers, rows, "Status dos Provedores de LLM");
}

void print_scan_results(const std::string& host, const std::vector<DiscoveredService>& services) {
    if (services.empty()) {
        std::cout << "\n" << ansi::BOLD_YELLOW << "Nenhum servidor de LLM ativo encontrado no host '" << host << "'." << ansi::RESET << "\n";
        std::cout << ansi::DIM << "Certifique-se de que o servidor (Ollama na porta 11434 ou llama.cpp na porta 8080) está rodando e acessível na rede." << ansi::RESET << "\n\n";
        return;
    }

    std::vector<std::string> headers = {"Serviço / Servidor", "Endpoint", "Modelos Disponíveis", "Comando para Usar"};
    std::vector<std::vector<std::string>> rows;

    for (const auto& s : services) {
        std::string models_text;
        if (s.models.empty()) {
            models_text = "(modelo padrão ativo)";
        } else {
            for (size_t i = 0; i < s.models.size(); ++i) {
                models_text += s.models[i] + (i + 1 < s.models.size() ? ", " : "");
            }
        }

        std::string cmd_example = "/model " + s.provider_type + "/" + (s.models.empty() ? "default" : s.models[0]);

        rows.push_back({
            std::string(ansi::BOLD_CYAN) + s.service_name + ansi::RESET,
            s.base_url,
            std::string(ansi::YELLOW) + models_text + ansi::RESET,
            std::string(ansi::BOLD_GREEN) + cmd_example + ansi::RESET
        });
    }

    ansi::print_table(headers, rows, "Serviços e Modelos Detectados em " + host);
}

} // namespace llmcli::ui
