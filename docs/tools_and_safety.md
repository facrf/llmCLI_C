# 🛡️ Ferramentas e Segurança - llmCli C++

O **llmCli C++** adota diretrizes estritas de segurança para garantir a integridade dos arquivos do desenvolvedor e o isolamento do workspace.

---

## 🔒 1. Política de Isolamento do Workspace

- **`is_path_safe`**: Todas as operações de leitura (`read_file`), escrita (`write_file`), busca (`grep_search`, `find_files`) e listagem (`list_dir`) são validadas para garantir que o caminho alvo reside estritamente dentro da raiz do projeto (`project_root`).
- Tentativas de acessar diretórios superiores como `/etc/`, `/tmp/` ou pastas pessoais fora do workspace são rejeitadas imediatamente.
- **Resiliência em Diretórios Restritos**: As ferramentas de busca e rastreamento ignoram pastas sem permissão de leitura (`skip_permission_denied`) sem abortar o processo.

---

## 📜 2. Conformidade com Diretrizes do Repositório (`AGENTS.md`)

- O **llmCli** carrega e injeta automaticamente as regras de conduta e restrições de escopo definidas no arquivo `AGENTS.md`, `CLAUDE.md`, `RULES.md` ou `.llmcli/AGENTS.md` da raiz do projeto.
- O desenvolvedor pode consultar as diretrizes ativas a qualquer momento através dos comandos `/agents` ou `/rules`.

---

## 🔒 3. Isolamento de Subprocessos e Estabilidade POSIX

- **Isolamento de `STDIN`**: Subprocessos executados em segundo plano (`run_git_cmd`, `run_command`) têm seu `STDIN` redirecionado para `/dev/null`, impedindo que comandos externos capturem teclas do terminal ou alterem os atributos de modo raw/cooked.
- **Tratamento de `SIGPIPE`**: A comunicação via pipes com instâncias do `curl` possui tratamento do sinal `SIGPIPE`, prevenindo que a queda ou encerramento abrupto do endpoint de rede cause o encerramento do processo pai.

---

## 💾 4. Checkpoints Git Automáticos & `/undo`

- Antes de aplicar modificações com `write_file` ou aplicar blocos `SEARCH/REPLACE`, o assistente cria automaticamente um commit de segurança no Git com prefixo `llmCli:`.
- Caso o resultado de uma modificação não seja o desejado, o usuário pode reverter instantaneamente o último estado utilizando o comando `/undo`.

---

## 🛠️ 5. Catálogo de Ferramentas Nativas

1. **`read_file`**: Lê o conteúdo de um arquivo de texto com numeração de linhas e suporte a intervalos.
2. **`write_file`**: Cria ou substitui arquivos no workspace de forma atômica.
3. **`list_dir`**: Lista a árvore de arquivos e diretórios respeitando filtros de exclusão.
4. **`grep_search`**: Busca de ocorrências de texto ou expressões regulares em todo o código.
5. **`find_files`**: Localização de arquivos por padrão glob (`*.hpp`, `test_*`).
6. **`run_command`**: Execução de comandos no terminal com timeout e bloqueio de comandos perigosos.
7. **`semantic_search`**: Busca semântica local via BM25 e pontuação de símbolos.
8. **`web_search`**: Pesquisa web via DuckDuckGo ou Tavily API.
9. **`read_url`**: Extração de texto limpo a partir de páginas e documentações web.
10. **`mcp_*`**: Ferramentas dinâmicas importadas de servidores Model Context Protocol.
