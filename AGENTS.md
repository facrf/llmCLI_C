# Diretrizes e Regras para Agentes (AGENTS.md) - llmCli C++

Este documento estabelece as regras obrigatórias de operação e restrição de escopo para qualquer agente de IA ou automação que interaja com a versão C++ do `llmCli` (`llmCLI_C`).

---

## 🔒 1. Restrição Estrita de Escopo (Workspace Boundary)

1. **Isolamento de Diretório:**
   - Todas as operações (leitura, escrita, edição, remoção, busca e compilação de código) **DEVEM** ser realizadas exclusivamente dentro deste diretório (`/storage/www/projetos/utils/llmCli/llmCLI_C`) e da raiz do projeto (`/storage/www/projetos/utils/llmCli`).
   - **NÃO** acesse, inspecione, crie, modifique ou exclua arquivos/pastas fora desta árvore de diretórios (por exemplo: `/tmp/`, `/home/`, `/var/`, `/etc/`, etc.).
   
2. **Execução de Comandos:**
   - Qualquer comando de terminal (`run_command`) deve ter seu diretório de trabalho (`Cwd`) configurado estritamente dentro de `/storage/www/projetos/utils/llmCli/llmCLI_C` ou `/storage/www/projetos/utils/llmCli`.
   - Não execute comandos que modifiquem o sistema operacional globalmente ou outros projetos.

3. **Arquivos Temporários e Artefatos:**
   - Quaisquer arquivos temporários, objetos de compilação (`build/`), binários (`bin/`) ou caches (`.cache/`) devem respeitar o `.gitignore`.

---

## 🛠️ 2. Boas Práticas e Padrões de Desenvolvimento C++

1. **Segurança e Chaves de API:**
   - **Nunca** versione ou exponha credenciais, chaves de API (OpenAI, Anthropic, Google Gemini, Groq, DeepSeek, Tavily) diretamente no código-fonte.
   - Utilize arquivos `.env` locais para configuração de credenciais e garanta que estejam listados no `.gitignore`.
   - Forneça sempre um arquivo de modelo `.env.example`.

2. **Qualidade e Estilo de Código C++:**
   - Utilize C++ moderno (C++20 / C++17) com semântica de movimento e RAII.
   - Gerencie recursos com smart pointers (`std::unique_ptr`, `std::shared_ptr`).
   - Mantenha tratamento de exceções robusto em todas as conexões HTTP/HTTPS e parsing JSON.
   - Garanta compatibilidade de compilação via `Makefile` (`make`, `make test`) e `CMakeLists.txt`.

3. **Integridade de Arquivos e Git Checkpoints:**
   - Ao alterar código existente, preserve comentários relevantes e formate adequadamente.
   - Valide modificações executando a suíte de testes (`make test`) antes de finalizar tarefas.
   - Utilize `is_path_safe` para garantir que qualquer operação em arquivo permaneça dentro dos limites do workspace.

---

## 📂 3. Estrutura de Pastas de `llmCLI_C`

```text
llmCLI_C/
├── .env.example          # Exemplo de variáveis de ambiente
├── .gitignore            # Arquivos ignorados pelo Git (build/, bin/, .env)
├── AGENTS.md             # Regras operacionais dos agentes (este arquivo)
├── CMakeLists.txt        # Configuração CMake do projeto
├── Makefile              # Makefile para build rápido e testes
├── README.md             # Documentação principal (Português)
├── README_EN.md          # Documentação em Inglês
├── bin/                  # Binários compilados (llm-cli, test_runner)
├── build/                # Objetos intermediários de compilação (.o)
├── docs/                 # Documentação técnica e arquitetural detalhada
├── include/              # Headers C++ (.hpp)
│   ├── llmcli/
│   │   ├── config.hpp
│   │   ├── i18n.hpp
│   │   ├── types.hpp
│   │   ├── context/      # file_tracker, repomap, semantic_indexer
│   │   ├── core/         # agent, diff_applier, exporter, session, todo_manager
│   │   ├── providers/    # gemini, anthropic, openai_compatible, llamacpp, ollama, scanner, registry
│   │   ├── tools/        # filesystem, git_ops, terminal, test_generator, web_tools, mcp_client
│   │   ├── ui/           # console, completer, repl
│   │   └── utils/        # ansi, env, http
│   └── nlohmann/         # json.hpp single-header
├── src/                  # Código-fonte C++ (.cpp)
└── tests/                # Suíte de testes unitários (.cpp)
```

---

## 💬 4. Comunicação e Links

- Forneça respostas claras, concisas e objetivas.
- Ao citar caminhos de arquivos e símbolos de código, utilize links clicáveis no padrão markdown (`[arquivo](file:///caminho/absoluto)`).
