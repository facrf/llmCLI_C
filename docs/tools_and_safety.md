# 🛡️ Ferramentas e Segurança - llmCli C++

O **llmCli C++** adota diretrizes estritas de segurança para garantir a integridade dos arquivos do desenvolvedor e o isolamento do workspace.

---

## 🔒 1. Política de Isolamento do Workspace

- **`is_path_safe`**: Operações de leitura (`read_file`), escrita (`write_file`), busca (`grep_search`, `find_files`), listagem (`list_dir`), exportação e persistência de sessão e persistência MCP validam que o caminho alvo reside estritamente dentro da raiz do projeto (`project_root`).
- Tentativas de acessar diretórios superiores como `/etc/`, `/tmp/` ou pastas pessoais fora do workspace são rejeitadas imediatamente.
- **Resiliência em Diretórios Restritos**: As ferramentas de busca e rastreamento ignoram pastas sem permissão de leitura (`skip_permission_denied`) sem abortar o processo.

---

## 📜 2. Conformidade com Diretrizes do Repositório (`AGENTS.md`)

- O **llmCli** carrega e injeta automaticamente as regras de conduta e restrições de escopo definidas no arquivo `AGENTS.md`, `CLAUDE.md`, `RULES.md` ou `.llmcli/AGENTS.md` da raiz do projeto.
- O desenvolvedor pode consultar as diretrizes ativas a qualquer momento através dos comandos `/agents` ou `/rules`.

---

## 🔒 3. Execução de comandos e estabilidade POSIX

- **Isolamento de `STDIN`**: Subprocessos executados em segundo plano (`run_git_cmd`, `run_command`) têm seu `STDIN` redirecionado para `/dev/null`, impedindo que comandos externos capturem teclas do terminal ou alterem os atributos de modo raw/cooked.
- **Falhas de rede**: Processos `curl` reportam erro e código HTTP pela camada HTTP; uma falha do endpoint não é interpretada como sucesso.
- **Comandos simples**: `run_command` aceita somente um comando com argumentos. Pipes, redirecionamentos, expansões, comandos encadeados, caminhos ascendentes e programas destrutivos são rejeitados. O processo inicia na raiz do workspace, possui timeout entre 1 e 3600 segundos e sua saída é limitada a 1 MiB.
- **Sem deadlock de saída**: `stdout` e `stderr` são drenados simultaneamente enquanto o processo está ativo. No timeout, todo o grupo de processos criado pelo comando é encerrado.
- **Limite importante**: isto não é um sandbox do sistema operacional. Um executável autorizado ainda pode acessar recursos permitidos ao usuário; para isolamento forte, execute o llmCli em contêiner, VM ou sandbox apropriado.

---

## 🌐 4. Transporte HTTP

- As requisições HTTP executam `curl` com argumentos estruturados (`execvp`), não por uma string de shell. URLs e cabeçalhos não são interpretados como código de shell.
- URLs são passadas após `--`, evitando que um valor iniciado por hífen seja tratado como opção do `curl`.

---

## 💾 5. Checkpoints Git Automáticos & `/undo`

- Antes de aplicar modificações com `write_file` ou aplicar blocos `SEARCH/REPLACE`, o assistente cria automaticamente um commit de segurança no Git com prefixo `llmCli:`.
- O `/undo` cria um commit `git revert` para o último checkpoint llmCli. Ele não usa `reset --hard` nem `git restore .`, preservando alterações não relacionadas na árvore de trabalho.

---

## 🛠️ 6. Catálogo de Ferramentas Nativas

1. **`read_file`**: Lê o conteúdo de um arquivo de texto com numeração de linhas e suporte a intervalos.
2. **`write_file`**: Cria ou substitui arquivos dentro do workspace após validação do caminho.
3. **`list_dir`**: Lista a árvore de arquivos e diretórios respeitando filtros de exclusão.
4. **`grep_search`**: Busca de ocorrências de texto ou expressões regulares em todo o código.
5. **`find_files`**: Localização de arquivos por padrão glob (`*.hpp`, `test_*`).
6. **`run_command`**: Execução de comandos no terminal com timeout e bloqueio de comandos perigosos.
7. **`semantic_search`**: Busca semântica local via BM25 e pontuação de símbolos.
8. **`web_search`**: Pesquisa web via DuckDuckGo ou Tavily API.
9. **`read_url`**: Extração de texto limpo a partir de páginas e documentações web.
10. **`mcp_*`**: Ferramentas dinâmicas importadas de servidores Model Context Protocol.

---

## 🧪 7. Validação

- `make test` compila e executa a suíte unitária.
- `make test-sanitize` recompila a suíte com AddressSanitizer e UndefinedBehaviorSanitizer.
- Em CMake, use `-DLLMCLI_ENABLE_SANITIZERS=ON` para habilitar os mesmos sanitizers; os testes são registrados no CTest como `llmcli_unit_tests`.
