# Fase 1: download de modelos + IA de texto dentro do PAUL3D — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Com só o PAUL3D aberto no console, o usuário baixa o modelo de texto pela página e gera uma peça na "Peça por texto", sem PS5LM e sem PC.

**Architecture:** O llama.cpp (só `src/` + `include/`) é compilado no título contra o ggml único da fase 0. Um módulo `mg_text` roda o modelo; o worker de `mg_main.cpp` ganha o modo `text` (modelo carregado entre pedidos, reinício ao sair do modo). Um módulo `mg_download` baixa do Hugging Face com o cliente HTTPS do PS5SD (mbedTLS), com retomada e sha256. O servidor web ganha `/api/text`, `/v1/chat/completions` e as rotas de download; a página ganha o painel Modelos e a `cad.html` passa a usar o próprio PAUL3D.

**Tech Stack:** C++17, llama.cpp @ `4ed1c8507e78c5dddc8e48d2822b87c2099e00d1`, ggml único (`scripts/ggml-unified.sh`), mbedTLS via `third_party/PS5SD/scripts/build-mbedtls.sh`, cpp-httplib (já no título), JS sem build, Python 3 (servidores de teste).

**Spec:** `docs/superpowers/specs/2026-11-07-paul3d-all-in-one-design.md` (fase 1; "Download de modelos pelo app"; "Um processo, um modo por vez"; "API").

## Global Constraints

- Repositório `github.com/powman/PAUL3D`, branch `main`, clone de trabalho limpo; não alterar `.github/workflows/build.yml` (token sem escopo `workflow`).
- Commits terminam com `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- llama.cpp fixo em `4ed1c8507e78c5dddc8e48d2822b87c2099e00d1` (ggml 0.27.0, tem a arquitetura `qwen35`); compilado contra o ggml único, nunca com o ggml interno dele.
- Modelo de texto padrão: `unsloth/Qwen3.5-9B-GGUF` rev `3885219b6810b007914f3a7950a8d1b469d598a5`, `Qwen3.5-9B-Q4_K_M.gguf`, 5680522464 bytes, sha256 `03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8`, Apache-2.0. Destino `/data/PAUL3D/models/text/`.
- Modelo de teste no PC: `unsloth/Qwen3.5-0.8B-GGUF` rev `6ab461498e2023f6e3c1baea90a8f0fe38ab64d0`, `Qwen3.5-0.8B-Q4_K_M.gguf`.
- Catálogo 3D: o mesmo de `tools/weights.sha256` (q4), destino `/data/PAUL3D/models/q4/`.
- Pastas lidas para texto, nesta ordem: `/data/PAUL3D/models/text/*.gguf`, depois `/data/PS5LM/models/*.gguf`.
- O título só acessa a rede quando o usuário pede um download; só `huggingface.co` (e os redirecionamentos dele para o CDN do HF); cada grupo exige `accept_license=true` na requisição.
- Modo `text` sai (e o título reinicia) quando chega um job de outro tipo ou depois de **300 s** sem pedido. Job `download` não reinicia o título.
- Textos da interface em português; código e comentários em inglês, como o resto do projeto.

## Review Focus

- Download interrompido (Wi-Fi cai, título fecha) e pedido de novo: continua do `.part`, não começa do zero, e o arquivo final só aparece depois do sha256 conferir. Teste na Task 6.
- Arquivo baixado com sha256 errado (CDN corrompeu, revisão trocada): apagado, status `failed` com o motivo, nunca usado. Teste na Task 6.
- Pedido de texto enquanto o título está reiniciando ou carregando o modelo (o servidor some por alguns segundos): a `cad.html` tenta de novo em vez de mostrar "sem conexão" na primeira falha. Teste na Task 8.
- Sem modelo de texto instalado: `/api/text` responde 409 com `missing_model` e a página leva ao painel Modelos, não a um erro genérico. Teste nas Tasks 5 e 8.
- Pouco espaço em disco para os 5,7 GB: o download recusa antes de começar, dizendo quanto falta. Teste na Task 6.

---

### Task 1: llama.cpp compilado contra o ggml único (PC)

**Files:**
- Create: `scripts/llama-fetch.sh`, `ps5/patch-llama-io.py`, `tools/test-llama/run.sh`, `tools/test-llama/hello.cpp`

**Interfaces:**
- Produces: `scripts/llama-fetch.sh <dest>` → clone mínimo do llama.cpp fixo em `<dest>` com `src/`, `include/`, `vendor/` e o patch de IO aplicado; marcador `<dest>/.paul3d-llama` (idempotente, mesmo padrão do `ggml-unified.sh`). `ps5/patch-llama-io.py <llama-dir>`: `setvbuf(f, nullptr, _IOFBF, 16 << 20)` depois do `fopen` de leitura em `src/llama-mmap.cpp` (o mesmo motivo do `patch-trellis-io.py`: a libc do console lê em pedaços pequenos); idempotente.

- [ ] **Step 1: Teste** — `tools/test-llama/run.sh`: baixa (cache em `${TMPDIR}`) o modelo de teste, monta o ggml único (`scripts/ggml-unified.sh`) e o llama (`scripts/llama-fetch.sh`), compila `hello.cpp` + `src/*.cpp` do llama + ggml (CPU) com CMake, roda `hello <gguf> "Reply with exactly the word OK."` e espera: saída 0; resposta contém `OK`; linha `arch=qwen35`. Segunda execução do `llama-fetch.sh` não muda nada.
- [ ] **Step 2:** Rodar → falha (scripts não existem).
- [ ] **Step 3:** Implementar os dois scripts e `hello.cpp` (carrega com `llama_model_load_from_file`, `use_mmap=false`, aplica o template de chat do próprio GGUF via `llama_chat_apply_template`, greedy, 32 tokens).
- [ ] **Step 4:** Rodar → `ok` em tudo. Se o `src/` do llama não compilar contra o ggml único: anotar os símbolos em "Resultado" e parar (o spec prevê plano B).
- [ ] **Step 5:** Commit.

### Task 2: `mg_text` — conversa com o modelo

**Files:**
- Create: `ps5/mg_text.hpp`, `ps5/mg_text.cpp`, `tools/test-llama/test_text.cpp`
- Modify: `tools/test-llama/run.sh` (compila e roda o teste)

**Interfaces:**
- Produces:
  ```cpp
  namespace mg {
  struct ChatMsg { std::string role, content; };            // role: system | user | assistant
  class TextEngine {
   public:
    bool load(const std::string & gguf, int n_ctx, int n_threads, std::string & err);  // 8192 de contexto por padrão
    bool loaded() const;
    // Gera a resposta; on_piece recebe cada pedaço (retorna false para parar). Pensamento (<think>) desligado.
    bool chat(const std::vector<ChatMsg> & msgs, int max_tokens, float temperature,
              const std::function<bool(const std::string &)> & on_piece, std::string & out, std::string & err);
    std::string model_name() const;                          // nome do arquivo
  };
  std::string find_text_model(const std::vector<std::string> & dirs);  // primeiro .gguf, ordem das pastas, nome em ordem alfabética; "" se nenhum
  }
  ```
- [ ] **Step 1: Testes** em `test_text.cpp` (assert simples, sem framework): `chat_ok` (temperatura 0, "Reply with exactly the word OK." → `out` contém `OK`); `pieces_join_to_out` (concatenação dos pedaços == `out`); `stop_early` (on_piece retorna false no 3º pedaço → retorna true e `out` tem só esses pedaços); `prompt_too_long` (mensagem de 20 000 palavras com contexto 512 → retorna false, `err` contém `context`); `no_think_block` (`out` não contém `<think>`); `find_text_model` (pasta temporária com `b.gguf`, `a.gguf` e `x.txt` → `a.gguf`; pastas vazias → `""`).
- [ ] **Step 2:** Rodar → falha (não compila).
- [ ] **Step 3:** Implementar. Pensamento desligado: depois do prompt montado pelo template, se o template do modelo contém `<think>`, acrescentar `<think>\n\n</think>\n\n`. Threads: o chamador passa (`paul3d_threads()` no console).
- [ ] **Step 4:** Rodar → todos `ok`.
- [ ] **Step 5:** Commit.

### Task 3: build do título com llama.cpp e `mg_text`

**Files:**
- Modify: `scripts/build-mg.sh` (chamar `llama-fetch.sh` para `$W/.deps/llama.cpp`; compilar `src/*.cpp` do llama e `ps5/mg_text.cpp` como os outros objetos; linkar)

- [ ] **Step 1:** Mudança no `build-mg.sh`; `bash -n` sem erro.
- [ ] **Step 2:** Commit e push de `main` (sem tag) → `gh run watch` → `title` com `success` e o log contém `llama-fetch:`. Falha de compilação/link: corrigir antes da Task 4 (ruling no ledger).

### Task 4: fila com tipos e o modo `text`

**Files:**
- Create: `ps5/mg_queue.hpp`, `ps5/mg_queue.cpp`, `tools/test-queue/test.cpp`, `tools/test-queue/run.sh`
- Modify: `ps5/mg_main.cpp` (worker usa `mg_queue`; modo texto com `mg_text`)

**Interfaces:**
- Produces:
  ```cpp
  namespace mg {
  enum class JobKind { mesh, text, download, unknown };
  JobKind kind_of(const std::string & file_name);      // "<id>.job" mesh, "<id>.textjob" text, "<id>.dljob" download
  std::string next_job(const std::vector<std::string> & names);   // menor nome entre os três tipos; "" se nenhum
  enum class Leave { stay, restart };
  // No modo texto: o que fazer com o próximo job (ou nenhum) depois de idle_s segundos parado.
  Leave text_mode_decision(JobKind next, double idle_s);           // text → stay; mesh/download → restart; none e idle_s >= 300 → restart
  }
  ```
  Job de texto: `/data/PAUL3D/jobs/<id>.textjob` com o JSON de `POST /api/text`; saída parcial em `/data/PAUL3D/text/<id>.txt` (reescrita a cada ~0,25 s), fim em `<id>.textdone` (`{"status":"done"|"failed","error":...}`).
- [ ] **Step 1: Testes** (`test.cpp`): `kind_of` dos três sufixos e de `x.tmp`; `next_job` com `w2.job`, `w1.textjob`, `w3.dljob` → `w1.textjob`; `text_mode_decision(text, 0)` → stay; `(mesh, 0)` → restart; `(download, 0)` → restart; `(unknown, 299)` → stay; `(unknown, 300)` → restart.
- [ ] **Step 2:** Rodar → falha.
- [ ] **Step 3:** Implementar `mg_queue` e ligar no `mg_main.cpp`: um job de texto carrega o modelo (`find_text_model`), atende os `.textjob` em ordem e só sai por `text_mode_decision`; o estado (`g_mg.stage`) mostra `texto: <modelo>`; sem modelo → `.textdone` com `{"status":"failed","error":"missing_model"}` e o título não reinicia.
- [ ] **Step 4:** Rodar os testes → `ok`; `g++ -fsyntax-only` do `mg_main.cpp` não é possível no PC (headers do console): conferido no build da Task 9.
- [ ] **Step 5:** Commit.

### Task 5: API de texto

**Files:**
- Modify: `ps5/mg_web.cpp`, `tools/test-web/run.sh`, `docs/API.md`

**Interfaces:**
- Produces:
  - `POST /api/text` JSON `{"messages":[{role,content}...],"max_tokens"?:4096,"temperature"?:0.2}` → `202 {"id"}`; `409 {"error":"missing_model"}` sem `.gguf` nas pastas; `400` para JSON inválido, `messages` vazio ou mais de 200 000 caracteres.
  - `GET /api/text/<id>` → `{"status":"queued"|"loading"|"generating"|"done"|"failed","text":"...","error"?}`.
  - `POST /v1/chat/completions` (OpenAI, sem stream): enfileira e segura até 600 s; resposta `{"choices":[{"message":{"role":"assistant","content":...},"finish_reason":"stop"}],"model":...}`; com `"stream":true` responde um único evento SSE com o texto todo + `[DONE]`.
  - `GET /v1/models` → `{"object":"list","data":[{"id":"<arquivo .gguf>"}]}` (vazio sem modelo).
- [ ] **Step 1: Testes** em `tools/test-web/run.sh` (docker `ps5sd-build`, como os de hoje): sem `.gguf` → `POST /api/text` 409 `missing_model`; com um `.gguf` falso em `/data/PAUL3D/models/text` → 202 e o arquivo `.textjob` existe com as mensagens; JSON inválido → 400; `GET /api/text/<id>` com `.txt` parcial → `generating` e o texto; com `.textdone` → `done`; `/v1/models` lista o arquivo; `/v1/chat/completions` com um worker falso (o teste escreve o `.txt` e o `.textdone` em 1 s) → 200 com o conteúdo; chave de API ligada → 401 nessas rotas.
- [ ] **Step 2:** Rodar → falha.
- [ ] **Step 3:** Implementar no `mg_web.cpp` (ids como os de mesh, `t` no lugar de `w`).
- [ ] **Step 4:** Rodar → `ok` em todos, inclusive os testes antigos do arquivo.
- [ ] **Step 5:** Commit.

### Task 6: download de modelos (`mg_download`)

**Files:**
- Create: `ps5/models.txt` (catálogo), `ps5/mg_download.hpp`, `ps5/mg_download.cpp`, `tools/test-download/test.cpp`, `tools/test-download/server.py`, `tools/test-download/run.sh`

**Interfaces:**
- Produces:
  - `ps5/models.txt`: uma linha por arquivo, `group sha256 size repo revision path dest licence-url`; grupos `mesh-q4` (as 10 linhas q4 de `tools/weights.sha256`) e `text-qwen35-9b` (a linha do Global Constraints).
  ```cpp
  namespace mg {
  struct ModelFile { std::string group, sha256, repo, rev, path, dest, licence; int64_t size; };
  std::vector<ModelFile> parse_catalog(const std::string & text, std::string & err);  // linhas # e vazias ignoradas
  std::string hf_url(const ModelFile & f);           // https://huggingface.co/<repo>/resolve/<rev>/<path>
  struct GroupState { std::string group; int64_t total, have; bool installed; };  // have: bytes em disco (final + .part)
  std::vector<GroupState> catalog_state(const std::vector<ModelFile> &, const std::string & root);
  // Baixa um arquivo para <root>/<dest>.part com Range a partir do tamanho já baixado, confere sha256, renomeia.
  // url: hf_url(f) no console; no teste, http://127.0.0.1:<porta>/... (o HttpClient do PS5SD aceita http).
  bool download_file(const ModelFile &, const std::string & root, const std::string & url,
                     const std::function<bool(int64_t done, int64_t total)> & progress, std::string & err);
  int64_t free_bytes(const std::string & path);
  }
  ```
- [ ] **Step 1: Testes** (`server.py` serve um arquivo com suporte a `Range`, pode cortar a conexão na metade e pode servir bytes errados; `test.cpp`): `parse_catalog` (11 linhas: 10 de `mesh-q4` + 1 de `text-qwen35-9b`, campos certos; linha com 7 campos → erro com número da linha); `hf_url`; `catalog_state` (nada, `.part` pela metade, arquivo final); `resume_after_cut` (servidor corta em 50 % → falha; segunda chamada continua do `.part`: o servidor registra `Range: bytes=<metade>-`; termina com sha certo); `bad_sha_deletes` (bytes errados → falha com `sha256`, nem `.part` nem final ficam); `no_space` (free_bytes simulado menor que o que falta → recusa antes de abrir conexão, mensagem com os GB que faltam).
- [ ] **Step 2:** Rodar → falha.
- [ ] **Step 3:** Implementar; sha256 com mbedTLS (`mbedtls_sha256`), que já vem com o cliente HTTPS; no PC o teste usa `http://127.0.0.1` sem TLS (o `HttpClient` aceita http).
- [ ] **Step 4:** Rodar → `ok`.
- [ ] **Step 5:** Commit.

### Task 7: downloads no título (rotas, worker, build)

**Files:**
- Modify: `ps5/mg_web.cpp`, `ps5/mg_main.cpp`, `scripts/build-mg.sh` (mbedTLS do PS5SD, `https_client.cpp`, `cacert.pem` e `models.txt` embutidos), `tools/test-web/run.sh`, `docs/API.md`

**Interfaces:**
- Produces: `GET /api/models` ganha `"groups":[{"group","title","size","installed","have","licence","needed_for":"3d"|"text"}]` (o que já existe fica igual); `POST /api/downloads` `{"group","accept_license":true}` → `202 {"id"}`, `400` sem `accept_license`, `409` se já instalado ou já na fila; `GET /api/downloads` → lista com `state` (`queued|downloading|verifying|done|failed|cancelled`), `done`, `total`, `speed`, `error`; `POST /api/downloads/<id>/cancel`. Worker: `.dljob` roda `download_file` para cada arquivo do grupo, escreve o estado em `/data/PAUL3D/downloads/<id>.json`, não reinicia o título.
- [ ] **Step 1: Testes** (`tools/test-web/run.sh`): `GET /api/models` tem os dois grupos com `installed:false`; `POST /api/downloads` sem `accept_license` → 400; com → 202 e o `.dljob`; o mesmo grupo de novo → 409; cancelar → `cancelled`.
- [ ] **Step 2:** Rodar → falha. **Step 3:** Implementar. **Step 4:** Rodar → `ok`.
- [ ] **Step 5:** Commit, push de `main` → `gh run watch` → `title` `success`.

### Task 8: a página — painel Modelos e "Peça por texto" com o PAUL3D

**Files:**
- Modify: `ps5/index.html` (painel Modelos), `ps5/cad.html` (servidor "PAUL3D (este console)"), `tools/test-cad/test.mjs`
- Create: `tools/test-cad/mock-paul3d.py` (as rotas novas, com estados roteirizados)

**Interfaces:**
- Consumes: rotas das Tasks 5 e 7.
- Produces (cad.html, lógica pura testável): `paul3dAsk(messages, {onPartial}) → Promise<string>` que faz `POST /api/text` e consulta `GET /api/text/<id>` a cada 700 ms; repete o POST/GET até 60 s quando a conexão cai (reinício do título); `missing_model` vira o erro `MISSING_MODEL`.
- [ ] **Step 1: Testes** — `test.mjs` (Node, `fetch` falso): `paul3dAsk` junta o texto parcial; 3 falhas de rede seguidas e depois sucesso → resolve; 409 `missing_model` → rejeita com `MISSING_MODEL`. Playwright contra `mock-paul3d.py`: na `cad.html` aberta pela porta do PAUL3D o servidor padrão é "PAUL3D (este console)"; gerar o chaveiro dá a peça; sem modelo, aparece "Baixe o modelo de texto" com link para o painel Modelos; no `index.html`, o painel mostra os dois grupos, o botão Baixar só habilita depois de marcar a licença, a barra de progresso anda e Cancelar funciona.
- [ ] **Step 2:** Rodar → falha. **Step 3:** Implementar. **Step 4:** Rodar → `ok`.
- [ ] **Step 5:** Commit.

### Task 9: v0.6.0 e teste no console

- [ ] **Step 1:** README (a promessa nova sobre rede, o painel Modelos, a IA de texto no console), `docs/API.md`, notas da 0.6.0, `contentVersion` `01.000.011`.
- [ ] **Step 2:** Commit, tag `v0.6.0`, push → `gh run watch` → `title` e `release` `success`.
- [ ] **Step 3:** Com o usuário (console em `192.168.0.154`): instalar; pelo painel Modelos baixar `text-qwen35-9b` (anotar velocidade); na `/cad`, gerar o chaveiro de exemplo só com o PAUL3D aberto (anotar tempo de carga do modelo, tokens/s do log, tempo total); depois gerar uma malha 3D (o título sai do modo texto e reinicia) e conferir que a malha é a de sempre (`compare-glb.py` contra `w20261107-192242-0`).
- [ ] **Step 4:** Resultados em "Resultado"; fase 1 marcada como concluída no spec.

## Resultado

(preenchido durante a execução)
