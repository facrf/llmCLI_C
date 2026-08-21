#include "llmcli/config.hpp"
#include "llmcli/core/agent.hpp"
#include "llmcli/core/session.hpp"
#include "llmcli/providers/registry.hpp"
#include "llmcli/providers/scanner.hpp"
#include "llmcli/ui/console.hpp"
#include "llmcli/ui/repl.hpp"
#include "llmcli/utils/ansi.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <csignal>

static const char* LLMCLI_VERSION = "0.2.0";

void print_help(const char* prog) {
    std::cout << "llmCli C++: Assistente IA de Código Híbrido (Local & Nuvem)\n\n"
              << "Uso: " << prog << " [opções] [prompt...]\n\n"
              << "Argumentos posicionais:\n"
              << "  prompt                 Prompt para execução direta não-interativa (one-shot).\n\n"
              << "Opções:\n"
              << "  -m, --model MODEL      Modelo de LLM a ser utilizado (ex: llamacpp/default, gemini/gemini-2.5-flash, openai/gpt-4o).\n"
              << "  -y, --yolo             Ativa o modo YOLO (execução autônoma total sem pedir confirmação).\n"
              << "  -f, --file FILE        Adiciona arquivo(s) ao contexto inicial (pode ser repetido).\n"
              << "  --scan IP              Escaneia um IP/host e detecta modelos e servidores ativos.\n"
              << "  --host IP              Conecta ao IP/host informado e configura endpoints locais.\n"
              << "  --models               Verifica e lista todos os provedores e modelos disponíveis.\n"
              << "  -v, --version          Exibe a versão do programa.\n"
              << "  -h, --help             Exibe esta mensagem de ajuda.\n";
}

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    std::vector<std::string> prompt_args;
    std::vector<std::string> files_to_add;
    std::string model_override;
    std::string scan_host;
    std::string host_ip;
    bool yolo_flag = false;
    bool list_models = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "llmCli C++ v" << LLMCLI_VERSION << std::endl;
            return 0;
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                model_override = argv[++i];
            }
        } else if (arg == "-y" || arg == "--yolo") {
            yolo_flag = true;
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                files_to_add.push_back(argv[++i]);
            }
        } else if (arg == "--scan") {
            if (i + 1 < argc) {
                scan_host = argv[++i];
            }
        } else if (arg == "--host") {
            if (i + 1 < argc) {
                host_ip = argv[++i];
            }
        } else if (arg == "--models") {
            list_models = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Opção desconhecida: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        } else {
            prompt_args.push_back(arg);
        }
    }

    llmcli::Config& cfg = llmcli::get_config();

    // 1. Scan IP and exit if --scan
    if (!scan_host.empty()) {
        std::cout << llmcli::ansi::DIM << "Escaneando host " << llmcli::ansi::BOLD_YELLOW << scan_host
                  << llmcli::ansi::RESET << llmcli::ansi::DIM << " em busca de modelos de LLM..." << llmcli::ansi::RESET << "\n";
        llmcli::providers::HostScanner scanner(scan_host);
        auto services = scanner.scan();
        llmcli::ui::print_scan_results(scan_host, services);
        return 0;
    }

    // 2. Connect to host if --host
    if (!host_ip.empty()) {
        std::cout << llmcli::ansi::DIM << "Conectando ao host " << llmcli::ansi::BOLD_YELLOW << host_ip
                  << llmcli::ansi::RESET << llmcli::ansi::DIM << " e detectando modelos..." << llmcli::ansi::RESET << "\n";
        llmcli::providers::HostScanner scanner(host_ip);
        auto services = scanner.scan();
        llmcli::ui::print_scan_results(host_ip, services);

        for (const auto& s : services) {
            if (s.provider_type == "ollama") cfg.local_endpoints.ollama = s.base_url;
            if (s.provider_type == "llamacpp") cfg.local_endpoints.llamacpp = s.base_url;
        }

        if (!services.empty() && model_override.empty()) {
            auto chosen = services[0];
            std::string m_name = chosen.models.empty() ? "default" : chosen.models[0];
            cfg.active_model = chosen.provider_type + "/" + m_name;
            std::cout << llmcli::ansi::BOLD_GREEN << "✓ Modelo ativo configurado automaticamente para: "
                      << llmcli::ansi::BOLD_YELLOW << cfg.active_model << llmcli::ansi::RESET << "\n\n";
        }
    }

    // Apply CLI overrides
    if (!model_override.empty()) {
        cfg.active_model = model_override;
    }
    if (yolo_flag) {
        cfg.yolo_mode = true;
    }

    // 3. List models and exit if --models
    if (list_models) {
        std::cout << llmcli::ansi::DIM << "Verificando provedores locais e em nuvem..." << llmcli::ansi::RESET << "\n";
        auto status = llmcli::providers::ProviderRegistry::get_status_overview();
        llmcli::ui::print_status_table(status);
        return 0;
    }

    auto session = std::make_shared<llmcli::core::Session>();

    // Add initial files from -f / --file
    for (const auto& f : files_to_add) {
        auto [ok, msg] = session->file_tracker().add_file(f);
        if (!ok) {
            std::cout << llmcli::ansi::YELLOW << "Aviso: " << msg << llmcli::ansi::RESET << "\n";
        }
    }

    auto agent = std::make_shared<llmcli::core::Agent>(session);

    // 4. One-shot direct execution if prompt was given
    if (!prompt_args.empty()) {
        std::ostringstream full_p;
        for (size_t i = 0; i < prompt_args.size(); ++i) {
            full_p << prompt_args[i] << (i + 1 < prompt_args.size() ? " " : "");
        }
        agent->run_prompt(full_p.str());
        return 0;
    }

    // 5. Interactive REPL Mode
    llmcli::ui::ReplSession repl(agent);
    repl.start();

    return 0;
}
