# 🔌 Integração com Model Context Protocol (MCP) - llmCli C++

O **llmCli C++** suporta o padrão aberto **Model Context Protocol (MCP)**, permitindo conectar assistentes de IA a ferramentas externas, bancos de dados, navegadores e servidores de contexto dinâmicos sem alterar o código-fonte do cliente.

---

## 📁 1. Arquivos de Configuração

O `llmCli` busca automaticamente as definições de servidores MCP nos seguintes locais:

1. **Configuração Local do Projeto:** `mcp_servers.json` ou `.llmcli/mcp.json` na raiz do projeto.
2. **Configuração Global do Usuário:** `~/.llmcli_mcp.json` na sua pasta pessoal (`$HOME`).

---

## 🛠️ 2. Estrutura do Arquivo `mcp_servers.json`

O arquivo utiliza formato JSON padrão com a lista de servidores e seus respectivos comandos de inicialização:

```json
{
  "mcpServers": {
    "sqlite": {
      "command": "uvx",
      "args": ["mcp-server-sqlite", "--db-path", "./database.sqlite"]
    },
    "fetch": {
      "command": "uvx",
      "args": ["mcp-server-fetch"]
    },
    "git": {
      "command": "uvx",
      "args": ["mcp-server-git", "--repository", "."]
    },
    "postgres": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-postgres", "postgresql://user:pass@localhost:5432/mydb"]
    },
    "brave-search": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-brave-search"],
      "env": {
        "BRAVE_API_KEY": "sua_chave_aqui"
      }
    }
  }
}
```

---

## 🚀 3. Exemplos Práticos de Servidores MCP

### A. Banco de Dados SQLite (`mcp-server-sqlite`)
Permite que o assistente consulte esquemas, execute consultas SQL (`SELECT`) e analise dados locais.
```json
{
  "mcpServers": {
    "sqlite": {
      "command": "uvx",
      "args": ["mcp-server-sqlite", "--db-path", "./app.db"]
    }
  }
}
```

### B. Leitura e Requisições Web (`mcp-server-fetch`)
Permite que a IA baixe conteúdos HTML/Markdown de páginas e documentações online para o contexto da sessão.
```json
{
  "mcpServers": {
    "fetch": {
      "command": "uvx",
      "args": ["mcp-server-fetch"]
    }
  }
}
```

### C. Servidor Git Especializado (`mcp-server-git`)
Fornece ferramentas avançadas de inspeção de repositório, diffs semânticos e histórico de branches.
```json
{
  "mcpServers": {
    "git": {
      "command": "uvx",
      "args": ["mcp-server-git", "--repository", "."]
    }
  }
}
```

---

## 🎮 4. Gerenciamento no REPL

- **Listar Servidores e Ferramentas Ativas:**
  ```text
  /mcp
  ```
  Exibe o status de conexão de cada servidor configurado e todas as ferramentas dinâmicas disponibilizadas para a LLM.

- **Execução com o Modo YOLO:**
  Ferramentas MCP declaradas são invocadas dinamicamente via Function Calling nativo dos provedores (Gemini, Claude, GPT-4o, Ollama, etc.). No modo `/yolo`, as ferramentas MCP rodam de forma autônoma contínua.
