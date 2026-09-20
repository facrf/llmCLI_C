# 🤖 Provedores e Modelos de LLM - llmCli C++

O **llmCli C++** possui arquitetura unificada para suportar inferência em LLMs locais e na nuvem.

---

## 🖥️ Provedores Locais

### 1. llama.cpp Server
- **Endpoint Padrão:** `http://localhost:8080`
- **Exemplo de Uso:** `./bin/llm-cli -m llamacpp/default` ou `/model llamacpp/default`
- **Características:** Altíssimo desempenho em C++, baixo consumo de memória, suporte a quantização GGUF e streaming SSE.

### 2. Ollama
- **Endpoint Padrão:** `http://localhost:11434`
- **Exemplo de Uso:** `./bin/llm-cli -m ollama`, `./bin/llm-cli -m ollama/qwen2.5-coder:7b` ou `/model ollama/deepseek-r1:latest`
- **Características:** Gestão simplificada de modelos locais e listagem automática via `/api/tags`.

O alias `ollama` seleciona corretamente o provedor Ollama e usa o modelo
padrão configurado. Para escolher uma tag específica, use sempre o formato
`ollama/<modelo>:<tag>`.

### 3. LM Studio
- **Endpoint Padrão:** `http://localhost:1234/v1`
- **Exemplo de Uso:** `/model lmstudio/default`

### 4. vLLM / LocalAI
- **Endpoint Padrão:** `http://localhost:8000/v1`
- **Exemplo de Uso:** `/model vllm/default`

---

## ☁️ Provedores em Nuvem

| Provedor | Variável no `.env` | Exemplo de Modelo |
| :--- | :--- | :--- |
| **Google Gemini** | `GEMINI_API_KEY` | `gemini/gemini-2.5-flash` ou `gemini/gemini-2.5-pro` |
| **OpenAI** | `OPENAI_API_KEY` | `openai/gpt-4o`, `openai/o3-mini`, `gpt/codex` |
| **Anthropic Claude**| `ANTHROPIC_API_KEY` | `anthropic/claude-3-7-sonnet-20250219` |
| **DeepSeek** | `DEEPSEEK_API_KEY` | `deepseek/deepseek-chat` ou `deepseek/deepseek-reasoner` |
| **Groq** | `GROQ_API_KEY` | `groq/llama-3.3-70b-versatile` |
| **OpenRouter** | `OPENROUTER_API_KEY` | `openrouter/anthropic/claude-3.5-sonnet` |
