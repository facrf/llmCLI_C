# 🤖 llmCli (Versão C++)

> **Language / Idioma:** 🇧🇷 [Português](README.md) | 🇺🇸 [English](README_EN.md)

**llmCli C++** é a reescrita nativa de altíssima performance em C++20 do assistente interativo de código com inteligência artificial para o terminal. Projetado com zero dependências externas obrigatórias, o sistema opera de forma ultraveloz com **LLMs locais** (llama.cpp, Ollama, LM Studio, vLLM) e **LLMs na nuvem** (Google Gemini, OpenAI GPT-4o / o-series, Anthropic Claude 3.7 / 3.5, DeepSeek V3 / R1, Groq, OpenRouter).

---

## 🌟 Principais Recursos

- 🔍 **Sugestões Dinâmicas em Tempo Real (Live Typeahead Popup):**
  - Ao digitar `/` no terminal, surge um popup flutuante interativo que filtra os comandos em tempo real com seleção por setas `[↑]`/`[↓]` e preenchimento com `[TAB]`/`[ENTER]`.
- 📜 **Auto-Injeção e Consulta de Diretrizes (`AGENTS.md` / `/agents` / `/rules`):**
  - Auto-descoberta de `AGENTS.md`, `CLAUDE.md` ou `RULES.md` na raiz do projeto e inclusão automática no System Prompt para garantir conformidade do agente com os padrões do repositório.
- ⚡ **Desempenho Nativo em C++20:**
  - Inicialização instantânea, baixíssimo uso de memória e consumo de CPU otimizado.
- 🔄 **Suporte Híbrido Completo:**
  - **Local:** **llama.cpp** (`http://localhost:8080`), **Ollama** (`http://localhost:11434`), **LM Studio** (`http://localhost:1234/v1`), **vLLM** (`http://localhost:8000/v1`).
  - **Nuvem:** **Google Gemini** (2.5 Flash / Pro), **OpenAI** (GPT-4o / o3-mini), **Anthropic Claude** (3.7 Sonnet / 3.5 Haiku), **DeepSeek** (V3 / R1), **Groq** e **OpenRouter**.
- 🏛️ **Modo Arquiteto (`/architect` / `/arch`):**
  - Pipeline de raciocínio duplo: modelo Arquiteto elabora a solução estruturada e o modelo Editor aplica as alterações no código.
- 🌐 **Internacionalização e Multi-idioma (`/lang`):**
  - Suporte nativo a 8 idiomas (**Português**, **English**, **Español**, **Deutsch**, **Français**, **简体中文**, **Русский**, **हिन्दी**) com auto-detecção via locale do SO (`/lang auto`).
- ⚡ **Modo YOLO (`/yolo` / `-y`):**
  - Execução autônoma contínua sem confirmações manuais a cada passo.
- 🧠 **Busca Semântica & RAG Local (`/index` / `/search`):**
  - Indexação inteligente de arquivos com pontuação BM25 e TF-IDF integrada, 100% offline.
- 🔌 **Extensibilidade via Model Context Protocol (`/mcp`):**
  - Conexão dinâmica com servidores MCP externos configurados em `mcp_servers.json` ou `~/.llmcli_mcp.json`.
- 🌐 **Pesquisa Web & Leitura de URLs (`/web` / `read_url`):**
  - Consulta informações atualizadas na web via DuckDuckGo ou Tavily API.
- 📋 **Checklist & Planejador de Tarefas (`/plan` / `/todo`):**
  - Criação automática de tarefas estruturadas a partir de objetivos em linguagem natural e acompanhamento do progresso em tempo real.
- 🧪 **Geração & Execução de Testes com Auto-Fix (`/gentest` / `/test`):**
  - Geração automática de suítes de testes unitários para múltiplas linguagens (Python, JS/TS, Go, Rust, PHP, Java, Ruby, C/C++) e execução de testes com diagnóstico de falhas via IA.
- 💾 **Preferências Persistentes do Usuário:**
  - Persistência automática do último modelo utilizado, modo YOLO, idioma e temperatura individual por modelo em `~/.llmcli_preferences.json`.
- 📄 **Exportação de Sessões (`/export`):**
  - Exporta relatórios técnicos completos da sessão em **Markdown** (`.md`) ou **HTML estilizado** interativo (`.html`).
- 🛡️ **Segurança Rigorosa & Git Checkpoints:**
  - Isolamento estrito de escopo ao diretório do projeto ([AGENTS.md](file:///storage/www/projetos/utils/llmCli/llmCLI_C/AGENTS.md)).
  - Auto-commit de segurança antes de cada edição para permitir reversão instantânea com o comando `/undo`.

---

## 🚀 Como Iniciar

### 1. Compilar o Projeto

```bash
# Compilar o executável bin/llm-cli
make

# Executar a suíte de testes unitários
make test
```

### 2. Configurar Variáveis de Ambiente

Copie o arquivo de modelo [.env.example](file:///storage/www/projetos/utils/llmCli/llmCLI_C/.env.example) para `.env`:

```bash
cp .env.example .env
```

Edite o arquivo `.env` com suas credenciais ou endpoints desejados:

```env
# Provedores em Nuvem
GEMINI_API_KEY=sua_chave_gemini
OPENAI_API_KEY=sua_chave_openai
ANTHROPIC_API_KEY=sua_chave_anthropic
DEEPSEEK_API_KEY=sua_chave_deepseek
GROQ_API_KEY=sua_chave_groq
OPENROUTER_API_KEY=sua_chave_openrouter

# Provedores Locais
LLAMACPP_BASE_URL=http://localhost:8080
OLLAMA_BASE_URL=http://localhost:11434
LMSTUDIO_BASE_URL=http://localhost:1234/v1
VLLM_BASE_URL=http://localhost:8000/v1

# Configurações Padrão
DEFAULT_MODEL=gemini/gemini-2.5-flash
ARCHITECT_MODEL=gemini/gemini-2.5-pro
LLMCLI_LANG=pt-BR
YOLO_MODE=false
```

O binário localiza este `.env` pela própria pasta de instalação, inclusive
quando é chamado por um alias de shell a partir de outro projeto. Variáveis
exportadas no ambiente continuam tendo prioridade. O comando `/host <ip>`
também salva os endpoints locais descobertos nas preferências do usuário.

### 3. Executar o llmCli C++

```bash
# Iniciar a interface interativa (REPL)
./bin/llm-cli

# Ou executar com um modelo específico
./bin/llm-cli -m llamacpp/default

# Alias curto para o Ollama (usa o modelo local padrão)
./bin/llm-cli -m ollama

# Conectar e escanear servidores em outra máquina da rede local
./bin/llm-cli --host 192.168.0.11 --yolo

# Execução direta não-interativa (one-shot batch)
./bin/llm-cli "Crie um algoritmo de ordenação rápida em C++"
```

---

## 🎮 Comandos Interativos (Slash Commands)

| Comando | Sintaxe | Descrição |
| :--- | :--- | :--- |
| **`/agents`** | `/agents` ou `/rules` | Exibe as diretrizes do arquivo `AGENTS.md` do projeto. |
| **`/yolo`** | `/yolo` | Alterna o modo autônomo YOLO. |
| **`/architect`** | `/architect [modelo]` | Alterna o Modo Arquiteto. |
| **`/dryrun`** | `/dryrun` ou `/dry-run` | Alterna o Modo Dry-Run (simulação sem gravação em disco). |
| **`/lang`** | `/lang <código>` | Altera o idioma (`pt`, `en`, `es`, `de`, `fr`, `zh`, `ru`, `hi`, `auto`). |
| **`/model`** | `/model <nome/id>` | Menu interativo ou troca de modelo. |
| **`/models`** | `/models` | Status de conectividade de todos os provedores. |
| **`/scan`** | `/scan <ip>` | Escaneia servidores de LLM em um IP. |
| **`/host`** | `/host <ip>` | Conecta ao IP informado como endpoint local. |
| **`/mcp`** | `/mcp` | Lista servidores MCP ativos. |
| **`/add`** | `/add <caminho>` | Adiciona arquivo/pasta ao contexto. |
| **`/drop`** | `/drop <caminho>` | Remove arquivo do contexto. |
| **`/files`** | `/files` | Lista arquivos anexados. |
| **`/index`** | `/index` | Indexa código para busca semântica. |
| **`/search`** | `/search <termo>` | Busca semântica/BM25 no código. |
| **`/web`** | `/web <pesquisa>` | Pesquisa na web via DuckDuckGo/Tavily. |
| **`/diff`** | `/diff` | Exibe alterações Git pendentes. |
| **`/commit`** | `/commit [msg]` | Cria commit semântico com IA ou mensagem direta. |
| **`/review`** | `/review` | Executa Code Review das alterações Git. |
| **`/undo`** | `/undo` | Reverte o último checkpoint realizado. |
| **`/test`** | `/test [args]` | Executa suíte de testes. |
| **`/gentest`** | `/gentest <arquivo>` | Gera testes unitários para o arquivo. |
| **`/run`** | `/run <comando>` | Executa um comando simples no workspace (sem pipes, redirecionamentos ou comandos encadeados). |
| **`/plan`** | `/plan <objetivo>` | Cria plano técnico e tarefas no `/todo`. |
| **`/todo`** | `/todo [add\|check\|clear]` | Gerencia o checklist de tarefas da sessão. |
| **`/export`** | `/export [md\|html]` | Exporta relatório da sessão em Markdown ou HTML. |
| **`/paste`** | `/paste` | Modo multilinha para colar blocos de código (`:done` para enviar). |
| **`/compact`** | `/compact` | Compacta histórico com resumo consolidado. |
| **`/temp`** | `/temp [valor]` | Altera a temperatura do modelo. |
| **`/system`** | `/system [prompt\|reset\|agents]` | Exibe, personaliza ou redefine o System Prompt. |
| **`/clear`** | `/clear` | Limpa o histórico de mensagens. |
| **`/reset`** | `/reset [prefs\|all]` | Limpa sessão ou redefine preferências do usuário. |
| **`/tokens`** | `/tokens` | Exibe consumo de tokens da sessão. |
| **`/help`** | `/help` | Exibe menu de ajuda. |
| **`/exit`** | `/exit` ou `/quit` | Encerra o assistente. |

---

## 📚 Documentação Detalhada

Explore os guias na pasta [`docs/`](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs):

- 🚀 [Guia de Início Rápido](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/getting_started.md)
- 🏗️ [Arquitetura do Sistema](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/architecture.md)
- ⚡ [Comandos Slash e Modo YOLO](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/commands_and_yolo.md)
- 🤖 [Provedores e Modelos de LLM](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/models_and_providers.md)
- 📡 [Descoberta Automática de Modelos por IP](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/network_discovery.md)
- 🔌 [Integração com Servidores MCP](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/mcp_servers.md)
- 🛡️ [Ferramentas e Segurança](file:///storage/www/projetos/utils/llmCli/llmCLI_C/docs/tools_and_safety.md)

---

## 🧪 Executando os Testes

```bash
make test
```

---

## 🔒 Diretrizes para Agentes de IA

Consulte o arquivo [AGENTS.md](file:///storage/www/projetos/utils/llmCli/llmCLI_C/AGENTS.md) para detalhes sobre isolamento de diretório e integridade de arquivos.
