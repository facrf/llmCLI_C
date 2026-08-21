# 🏗️ Arquitetura do Sistema - llmCli C++

O **llmCli C++** é estruturado em camadas modulares de alta performance com gerenciamento seguro de memória via RAII e smart pointers.

---

## 📐 Diagrama de Componentes

```text
┌────────────────────────────────────────────────────────┐
│                   CLI Entrypoint (main)                │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                 REPL & Console UI                      │
│     (Slash Commands, Autocompleter, Rich ANSI)         │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                    Agent & Pipeline                    │
│   (Loop de Raciocínio, Architect/Editor, Token Count)  │
├───────────────────────────┬────────────────────────────┤
│         Session           │        Diff Applier        │
│   (Histórico, Contexto)   │    (SEARCH/REPLACE, Fuzzy) │
└─────────────┬─────────────┴─────────────┬──────────────┘
              │                           │
              ▼                           ▼
┌───────────────────────────┐ ┌──────────────────────────┐
│      Context Engine       │ │       Tools Engine       │
│  - File Tracker           │ │  - Filesystem (read/wr)  │
│  - RepoMap Generator      │ │  - Git Ops & Checkpoints │
│  - Semantic Indexer(BM25) │ │  - Terminal Runner       │
│                           │ │  - Web Search & URL      │
│                           │ │  - MCP Client            │
└─────────────┬─────────────┘ └───────────┬──────────────┘
              │                           │
              └─────────────┬─────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   Provider Registry                    │
│    (OpenAI, Gemini, Anthropic, llama.cpp, Ollama, ...) │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                  HTTP & Network Layer                  │
│       (Streaming SSE, Scanner de Rede, JSON Parser)    │
└────────────────────────────────────────────────────────┘
```

---

## 📂 Descrição dos Módulos

1. **`include/llmcli/core/` & `src/core/`**:
   - `Agent`: Loop de execução contínua, orquestração de chamadas de ferramentas e modo Arquiteto.
   - `Session`: Histórico de mensagens, formatação de prompt de sistema e compactação de tokens.
   - `DiffApplier`: Parser de blocos SEARCH/REPLACE, correspondência fuzzy e aplicação de patches unificados.
   - `TodoManager`: Checklist interativo e extração de tarefas estruturadas de planos.
   - `Exporter`: Exportação de relatórios em Markdown (.md) e HTML estilizado (.html).

2. **`include/llmcli/context/` & `src/context/`**:
   - `FileTracker`: Rastreamento de arquivos ativos e renderização com numeração de linhas.
   - `RepoMap`: Visualização em árvore concisa da base de código do projeto.
   - `SemanticIndexer`: Indexador BM25 / TF-IDF offline com suporte a classes, funções e blocos.

3. **`include/llmcli/providers/` & `src/providers/`**:
   - `LLMProvider`: Interface abstrata para provedores.
   - `OpenAICompatibleProvider`: Compatível com OpenAI, LM Studio, vLLM, DeepSeek, Groq, OpenRouter.
   - `GeminiProvider`: Google Gemini SSE e declaração nativa de ferramentas.
   - `AnthropicProvider`: Claude 3.7 / 3.5 Messages API.
   - `LlamaCppProvider` & `OllamaProvider`: Servidores locais de inferência.
   - `HostScanner`: Descoberta assíncrona de servidores e modelos na rede.
   - `ProviderRegistry`: Resolução de modelos e presets.

4. **`include/llmcli/tools/` & `src/tools/`**:
   - `Filesystem`: `read_file`, `write_file`, `list_dir`, `grep_search`, `find_files`.
   - `GitOps`: auto-commits de segurança, `undo` e diffs.
   - `Terminal`: `run_command` com timeout e proteções.
   - `WebTools`: `web_search` e `read_url`.
   - `McpClient`: Conexão com servidores MCP externos.
