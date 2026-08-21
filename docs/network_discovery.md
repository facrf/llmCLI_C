# 📡 Descoberta Automática de Modelos por IP - llmCli C++

O **llmCli C++** possui um subsistema assíncrono de varredura de rede capaz de localizar e conectar nós de inferência de IA disponíveis na sua máquina ou em servidores dedicados na rede local (LAN).

---

## 🔍 Como Utilizar

### 1. Escanear um IP via CLI

```bash
# Escanear host local
./bin/llm-cli --scan 127.0.0.1

# Escanear servidor remoto de IA na rede
./bin/llm-cli --scan 192.168.0.11
```

### 2. Conectar e Configurar como Endpoint Ativo

```bash
# Conecta ao servidor e seleciona automaticamente o modelo detectado
./bin/llm-cli --host 192.168.0.11
```

### 3. Durante a Sessão Interativa (Slash Commands)

```text
/scan 192.168.0.11
/host 192.168.0.11
```

---

## 🔎 Portas e Serviços Verificados

- **Ollama:** Porta `11434` (`/api/version` e `/api/tags`)
- **llama.cpp Server:** Porta `8080` e `8081` (`/props` e `/v1/models`)
- **LM Studio:** Porta `1234` (`/v1/models`)
- **vLLM / LocalAI:** Porta `8000` (`/v1/models`)
- **Text-Gen-WebUI:** Porta `5000` (`/v1/models`)
