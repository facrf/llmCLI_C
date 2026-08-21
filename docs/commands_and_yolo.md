# ⚡ Comandos Slash e Modo YOLO - llmCli C++

O **llmCli C++** disponibiliza uma rica interface de linha de comando com sugestões interativas em tempo real (popup live typeahead), atalhos slash (`/`) e autocomplete inteligente via tecla `Tab`.

---

## 🔍 Sugestões Dinâmicas em Tempo Real (Live Typeahead Popup)

Ao digitar `/` no prompt interativo, um menu flutuante surge **instantaneamente** abaixo do cursor exibindo comandos compatíveis com breves descrições:

- **Filtro em Tempo Real:** Conforme você digita (ex: `/m`, `/mod`, `/ag`), a lista de sugestões é filtrada instantaneamente.
- **Navegação Interativa (`[↑]` / `[↓]`):** Navegue entre as opções com as setas para cima e para baixo (`›`).
- **Preenchimento Rápido (`[TAB]` / `[ENTER]`):** Pressione `Tab` ou `Enter` para selecionar o comando destacado.
- **Grid Alinhado no `Tab`:** Pressionar `Tab` com prefixo ambíguo renderiza uma grade alinhada respeitando a largura do terminal.

---

## 📋 Tabela Geral de Comandos

| Comando | Sintaxe | Descrição |
| :--- | :--- | :--- |
| **`/agents`** | `/agents` ou `/rules` | Exibe as regras e diretrizes do arquivo `AGENTS.md` (ou `CLAUDE.md` / `RULES.md`). |
| **`/yolo`** | `/yolo` | Alterna o modo autônomo YOLO (salva preferência por modelo e global). |
| **`/architect`** | `/architect [modelo]` | Alterna o Modo Arquiteto (planejamento forte + editor ágil). Atalho: `/arch`. |
| **`/dryrun`** | `/dryrun` ou `/dry-run` | Alterna o Modo Dry-Run (simulação sem gravação em disco ou execução de shell). |
| **`/lang`** | `/lang <código>` | Altera o idioma do sistema (`pt`, `en`, `es`, `de`, `fr`, `zh`, `ru`, `hi`, `auto`). |
| **`/model`** | `/model <nome/id>` | Menu interativo ou troca direta de modelo (ex: `/model llamacpp/default`). |
| **`/models`** | `/models` | Exibe status de conectividade e saúde de todos os provedores. |
| **`/scan`** | `/scan <ip>` | Escaneia um IP/host e detecta servidores e modelos de LLM ativos. |
| **`/host`** | `/host <ip>` | Conecta ao IP informado e configura endpoints locais automaticamente. |
| **`/mcp`** | `/mcp` | Lista servidores MCP e ferramentas dinâmicas ativas. |
| **`/add`** | `/add <caminho>` | Adiciona arquivo ou pasta ao contexto da IA (com autocomplete `Tab`). |
| **`/drop`** | `/drop <caminho>` | Remove arquivo do contexto ativo. |
| **`/files`** | `/files` | Lista todos os arquivos atualmente anexados ao contexto. |
| **`/index`** | `/index` | Indexa a base de código para busca semântica/RAG local. |
| **`/search`** | `/search <termo>` | Executa busca semântica/BM25 no código indexado. |
| **`/web`** | `/web <pesquisa>` | Pesquisa na web (DuckDuckGo/Tavily) e traz respostas atualizadas. |
| **`/diff`** | `/diff` | Exibe as alterações Git pendentes com visualizador colorido. |
| **`/commit`** | `/commit [msg]` | Gera mensagem de commit semântica com IA ou cria commit direto. |
| **`/review`** | `/review` | Executa Code Review técnico das alterações Git pendentes. |
| **`/undo`** | `/undo` | Reverte o último checkpoint / alteração realizada pela IA. |
| **`/test`** | `/test [args]` | Executa testes e sugere correção automática caso ocorra falha. |
| **`/gentest`** | `/gentest <arquivo>` | Gera suíte de testes unitários multi-linguagem (Py, JS/TS, Go, Rust, PHP, Java, Ruby, C/C++). |
| **`/run`** | `/run <comando>` | Executa um comando de terminal diretamente no workspace. |
| **`/plan`** | `/plan <objetivo>` | Cria plano técnico estruturado e gera tarefas automáticas no `/todo`. |
| **`/todo`** | `/todo [add\|check\|clear]` | Gerencia o checklist interativo de tarefas da sessão. |
| **`/export`** | `/export [md\|html]` | Exporta relatório técnico completo da sessão em Markdown ou HTML. |
| **`/paste`** | `/paste` | Modo multilinha para colar grandes blocos de código (`:done` para enviar). |
| **`/compact`** | `/compact` | Compacta o histórico da conversa gerando resumo consolidado. |
| **`/temp`** | `/temp [valor]` | Exibe ou altera a temperatura do modelo ativo (salva preferência). |
| **`/system`** | `/system [prompt\|reset\|agents]` | Exibe, personaliza ou redefine o System Prompt do assistente. |
| **`/clear`** | `/clear` | Limpa o histórico de mensagens mantendo os arquivos no contexto. |
| **`/reset`** | `/reset [prefs\|all]` | Limpa sessão ou redefine preferências salvas do usuário para o padrão. |
| **`/tokens`** | `/tokens` | Exibe estimativa de consumo de tokens do contexto e da sessão. |
| **`/help`** | `/help` | Exibe o menu com todos os comandos disponíveis. |
| **`/exit`** | `/exit` ou `/quit` | Encerra o assistente. |

---

## 📜 Regras do Projeto e `AGENTS.md`

O **llmCli** descobre automaticamente arquivos de diretrizes presentes na raiz do projeto (`AGENTS.md`, `CLAUDE.md`, `RULES.md` ou `.llmcli/AGENTS.md`) e injeta suas regras no System Prompt.

- Para consultar as regras do projeto a qualquer momento, execute `/agents` ou `/rules`.
- Para inspecionar o System Prompt completo ativo enviado para a LLM, execute `/system`.

---

## ⚡ Modo YOLO (`/yolo`)

No modo padrão, o llmCli solicita confirmação interativa antes de executar modificações em arquivos (`write_file`) ou rodar comandos de terminal (`run_command`).

Ao ativar o **Modo YOLO** (`/yolo` ou via flag `-y` na inicialização):
- Todas as operações em arquivos e comandos de terminal são executados autonomamente em tempo real.
- A preferência do Modo YOLO é salva e persistida automaticamente em `~/.llmcli_preferences.json`.
