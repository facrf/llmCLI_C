# 🤖 llmCli (C++ Version)

> **Language / Idioma:** 🇺🇸 [English](README_EN.md) | 🇧🇷 [Português](README.md)

**llmCli C++** is a high-performance native C++20 rewrite of the interactive AI coding assistant for the terminal. Built with zero mandatory external dependencies, it operates with blazing speed across **local LLMs** (llama.cpp, Ollama, LM Studio, vLLM) and **cloud LLMs** (Google Gemini, OpenAI GPT-4o / o-series, Anthropic Claude 3.7 / 3.5, DeepSeek V3 / R1, Groq, OpenRouter).

---

## 🌟 Key Features

- 🔍 **Live Typeahead Suggestions Popup:** Typing `/` in the prompt opens an interactive floating menu that dynamically filters matching slash commands with `[↑]`/`[↓]` arrow navigation and `[TAB]`/`[ENTER]` completion.
- 📜 **Automatic Project Rules Injection (`AGENTS.md` / `/agents` / `/rules`):** Auto-detects `AGENTS.md`, `CLAUDE.md`, or `RULES.md` in the project root and automatically injects rules into the System Prompt.
- ⚡ **Native C++20 Performance:** Instant startup, minimal memory footprint, and optimized CPU usage.
- 🔄 **Full Hybrid Support:**
  - **Local:** llama.cpp (`http://localhost:8080`), Ollama (`http://localhost:11434`), LM Studio (`http://localhost:1234/v1`), vLLM (`http://localhost:8000/v1`).
  - **Cloud:** Google Gemini (2.5 Flash / Pro), OpenAI (GPT-4o / o3-mini), Anthropic Claude (3.7 Sonnet / 3.5 Haiku), DeepSeek (V3 / R1), Groq, OpenRouter.
- 🏛️ **Architect Mode (`/architect` / `/arch`):** Dual-pipeline with reasoning model as Architect and fast model as Editor.
- 🌐 **Internationalization (`/lang`):** 8 supported languages (**Português**, **English**, **Español**, **Deutsch**, **Français**, **简体中文**, **Русский**, **हिन्दी**) + OS auto-detection (`/lang auto`).
- ⚡ **YOLO Mode (`/yolo` / `-y`):** Fully autonomous tool execution without manual confirmations.
- 🧠 **Semantic Code Search & Local RAG (`/index` / `/search`):** Offline BM25 and TF-IDF indexed search.
- 🔌 **Model Context Protocol Support (`/mcp`):** Connects to external MCP servers defined in `mcp_servers.json`.
- 🌐 **Web Search & URL Content Extraction (`/web` / `read_url`):** Online lookups via DuckDuckGo or Tavily API.
- 📋 **Checklist & Task Planner (`/plan` / `/todo`):** Natural language plan breakdown and real-time task tracking.
- 🧪 **Unit Test Generation & Auto-Fix (`/gentest` / `/test`):** Automated test generation for multiple programming languages (Python, JS/TS, Go, Rust, PHP, Java, Ruby, C/C++) and failure diagnosis.
- 💾 **Persistent User Preferences:** Stored in `~/.llmcli_preferences.json`.
- 📄 **Session Exporter (`/export`):** Markdown (`.md`) and styled HTML (`.html`) reports.
- 🛡️ **Strict Workspace Isolation & Git Checkpoints:** Safe workspace boundary with `/undo` rollback support.

---

## 🚀 Quickstart

### 1. Build the Project

```bash
# Build bin/llm-cli
make

# Run test suite
make test
```

### 2. Configure Environment Variables

```bash
cp .env.example .env
```

### 3. Run llmCli C++

```bash
# Interactive REPL
./bin/llm-cli

# Run with specific model
./bin/llm-cli -m llamacpp/default

# Autonomous YOLO execution
./bin/llm-cli -y

# One-shot batch prompt
./bin/llm-cli "Create a high-performance LRU cache in C++"
```

---

## 🎮 Interactive Slash Commands

| Command | Syntax | Description |
| :--- | :--- | :--- |
| **`/agents`** | `/agents` or `/rules` | Displays project rules from `AGENTS.md`. |
| **`/yolo`** | `/yolo` | Toggles autonomous YOLO execution mode. |
| **`/architect`** | `/architect [model]` | Toggles Architect Mode (planner + editor). |
| **`/dryrun`** | `/dryrun` or `/dry-run` | Toggles Dry-Run mode (simulation without writing files). |
| **`/lang`** | `/lang <code>` | Changes UI language (`pt`, `en`, `es`, `de`, `fr`, `zh`, `ru`, `hi`, `auto`). |
| **`/model`** | `/model <name/id>` | Interactive model picker or switch. |
| **`/models`** | `/models` | Status overview of all configured providers. |
| **`/scan`** | `/scan <ip>` | Scans local host/IP for active LLM servers. |
| **`/host`** | `/host <ip>` | Connects to remote host as local endpoint. |
| **`/mcp`** | `/mcp` | Lists active MCP servers and dynamic tools. |
| **`/add`** | `/add <path>` | Adds file or directory to AI context. |
| **`/drop`** | `/drop <path>` | Removes file from active context. |
| **`/files`** | `/files` | Lists files currently loaded in context. |
| **`/index`** | `/index` | Indexes codebase for local semantic BM25 search. |
| **`/search`** | `/search <query>` | Performs semantic/BM25 code search. |
| **`/web`** | `/web <query>` | Web search via DuckDuckGo / Tavily API. |
| **`/diff`** | `/diff` | Displays pending Git changes with colored diff. |
| **`/commit`** | `/commit [msg]` | Creates AI semantic commit or direct message. |
| **`/review`** | `/review` | Performs AI Code Review on pending Git changes. |
| **`/undo`** | `/undo` | Reverts the last checkpoint / AI edit. |
| **`/test`** | `/test [args]` | Runs test suite and suggests automated fixes. |
| **`/gentest`** | `/gentest <file>` | Generates complete unit tests for target file. |
| **`/run`** | `/run <cmd>` | Executes terminal command within workspace. |
| **`/plan`** | `/plan <goal>` | Generates technical plan and checklist items in `/todo`. |
| **`/todo`** | `/todo [add\|check\|clear]` | Manages session task checklist. |
| **`/export`** | `/export [md\|html]` | Exports session transcript to Markdown or HTML. |
| **`/paste`** | `/paste` | Multiline paste mode (`:done` to submit). |
| **`/compact`** | `/compact` | Compacts session history with consolidated summary. |
| **`/temp`** | `/temp [val]` | Sets model temperature. |
| **`/system`** | `/system [prompt\|reset\|agents]` | Inspects, modifies, or resets the active System Prompt. |
| **`/clear`** | `/clear` | Clears conversation history. |
| **`/reset`** | `/reset [prefs\|all]` | Clears session or resets user preferences. |
| **`/tokens`** | `/tokens` | Displays token usage estimation. |
| **`/help`** | `/help` | Displays help menu. |
| **`/exit`** | `/exit` or `/quit` | Quits the assistant. |
