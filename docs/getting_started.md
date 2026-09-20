# 🚀 Guia de Início Rápido - llmCli C++

Este guia explica como compilar, configurar e executar o **llmCli C++** no seu ambiente local.

---

## 📦 1. Requisitos do Sistema

- **Compilador C++:** `g++` ou `clang++` com suporte a C++20 (GCC 11+ ou Clang 13+).
- **Ferramentas de Build:** `make` ou `cmake` (3.16+).
- **Rede / HTTP:** utilitário `curl` instalado no sistema operacional.
- **Git:** para suporte a checkpoints automáticos e controle de versão.

---

## 🔨 2. Compilação do Projeto

Dentro da pasta `llmCLI_C/`:

### Opção A: Usando Makefile (Recomendado)

```bash
# Compilar o executável principal (bin/llm-cli)
make

# Compilar e rodar a suíte de testes unitários
make test
```

### Opção B: Usando CMake

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
cd ..
```

---

## ⚙️ 3. Configuração de Variáveis de Ambiente (.env)

Copie o arquivo de exemplo [.env.example](file:///storage/www/projetos/utils/llmCli/llmCLI_C/.env.example) para `.env`:

```bash
cp .env.example .env
```

Edite o arquivo `.env` com suas chaves de API ou endereços dos servidores locais:

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

### Uso por alias e prioridade de configuração

O binário encontra o `.env` na pasta onde foi instalado, portanto a
configuração não muda quando ele é chamado por um alias de shell em outro
projeto. A precedência é: variáveis já exportadas no shell, `.env` da
instalação, `.env` do workspace e preferências persistentes.

Para usar um servidor local remoto sem editar o `.env`, conecte uma vez com
`--host <ip>` ou `/host <ip>`. Os endpoints detectados são salvos nas
preferências do usuário e serão reutilizados nas execuções seguintes.

---

## 🎮 4. Executando o llmCli

```bash
# Iniciar o REPL interativo
./bin/llm-cli

# Iniciar com modelo local llama.cpp
./bin/llm-cli -m llamacpp/default

# Alias curto para o provedor Ollama (modelo local padrão)
./bin/llm-cli -m ollama

# Iniciar no modo autônomo YOLO
./bin/llm-cli -y

# Conectar e escanear modelos em outro IP da rede local
./bin/llm-cli --host 192.168.0.11

# Execução direta de prompt (One-shot batch)
./bin/llm-cli "Crie um programa em C++ que calcule a sequência de Fibonacci"
```
