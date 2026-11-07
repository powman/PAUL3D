# Fase 0: ggml único — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O PAUL3D compila o motor 3D com o ggml que o stable-diffusion.cpp usa (base comum dos três motores), sem mudar o resultado nem a velocidade do 3D.

**Architecture:** Um script monta o ggml único (leejet/ggml fixado + as correções do pwilkin/ggml + os patches de ggml do PS5SD). Uma validação no PC (CPU) compara a malha do ggml atual com a do ggml único, mesma foto e seed. Só então o `build-mg.sh` passa a usar o ggml único, e sai a v0.5.0 para o teste no console.

**Tech Stack:** bash, git, CMake, trellis.cpp @ `8e4ce11`, Python 3 (stdlib) para comparar GLB, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-07-paul3d-all-in-one-design.md` (seção "Um ggml para os três motores", fase 0).

## Global Constraints

- Repositório: `github.com/powman/PAUL3D`, branch `main`. Clone de trabalho: um clone limpo do repositório (a pasta local `PS5MG-main` não é git; copie as mudanças para o clone como foi feito no commit `0ee9a33`).
- O token do `gh` não tem o escopo `workflow`: não altere `.github/workflows/build.yml`.
- trellis.cpp continua fixado em `8e4ce11a77d35c3e3a4b3f84a29d92bf5491e64e`.
- ggml único = `https://github.com/leejet/ggml` @ `dc998771e700d49bbdf9a39cede22bb5b3311767` (upstream ggml 0.27.0 + 13 commits do leejet) + os 10 commits de `https://github.com/pwilkin/ggml` entre `456172ec` e `f8697b33` + `third_party/PS5SD/patches/ggml/*.patch`.
- Commits terminam com `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Aceitação da malha (PC e console): mesmo número de faces ±1 %, cada lado da bounding box ±1 %, mesma seed e foto.

## Review Focus

- Um commit do pwilkin que não aplica sobre o leejet (conflito): o script tem de parar com mensagem clara dizendo qual commit, não seguir com um ggml pela metade. Teste na Task 2.
- O script rodado de novo sobre uma pasta já montada (o build do CI reusa `.deps`): não pode aplicar patch duas vezes. Teste na Task 2.
- trellis.cpp não compila contra ggml 0.27 (API mudou entre 0.24 e 0.27): a Task 3 para e registra o erro; não se remenda a API no trellis.cpp sem registrar no plano B.
- Malha igual mas tempo pior no console (ex.: um kernel Vulkan novo mais lento): a Task 5 compara os tempos por etapa com `tools/runinfo.py`, não só a malha.
- GLB com mais de uma malha ou com nós transformados: o comparador soma todas as malhas e aplica as transformações dos nós. Teste na Task 1.

---

### Task 0: Publicar a v0.4.0 (base de comparação)

**Files:**
- Modify (no clone): `ps5/cad.html` (novo), `ps5/index.html`, `ps5/mg_web.cpp`, `scripts/build-mg.sh`, `tools/test-cad/test.mjs` (novo), `tools/test-web/run.sh`, `README.md`, `docs/API.md`, `docs/RELEASE-NOTES.md`, `sce_sys/param.json`, `docs/superpowers/**`

- [ ] **Step 1:** Copiar as mudanças da pasta local para o clone (rsync excluindo `.git`, `third_party`, `.impeccable`, `__pycache__`, `build`), sem `.github/workflows/build.yml`.
- [ ] **Step 2:** `node tools/test-cad/test.mjs` → 17 linhas `ok`.
- [ ] **Step 3:** Notas da 0.4.0 no topo de `docs/RELEASE-NOTES.md` (Peça por texto, ajuda de instalação da IA, logo novo) e `contentVersion` `01.000.009` em `sce_sys/param.json`.
- [ ] **Step 4:** Commit "Release 0.4.0", tag `v0.4.0`, push de `main` e da tag.
- [ ] **Step 5:** `gh run watch <id> --repo powman/PAUL3D --exit-status` → jobs `title` e `release` com `success`; `gh release view v0.4.0` lista `PPSA99577.zip`.

### Task 1: Comparador de GLB

**Files:**
- Create: `tools/compare-glb.py`
- Test: `tools/test-compare/run.sh` (gera dois GLB mínimos com Python e chama o comparador)

**Interfaces:**
- Produces: `python3 tools/compare-glb.py A.glb B.glb [--tol 0.01]` → imprime `faces A/B`, `bbox A/B` e `PASS` ou `FAIL: <motivo>`; código de saída 0 em PASS, 1 em FAIL, 2 em arquivo inválido. Só stdlib (`struct`, `json`).

- [ ] **Step 1: Teste** — `tools/test-compare/run.sh` escreve com Python stdlib: `cube.glb` (12 faces, lado 1), `cube2.glb` (igual), `cube_big.glb` (lado 1,05), `two.glb` (duas malhas de 12 faces, uma num nó com `translation [2,0,0]`) e espera: cube×cube2 → saída 0 e `PASS`; cube×cube_big → saída 1 e `FAIL: bbox`; cube×two → saída 1 e `FAIL: faces 12 vs 24`; `two.glb` bbox x = 3,0; arquivo de texto → saída 2.
- [ ] **Step 2:** `bash tools/test-compare/run.sh` → falha (comparador não existe).
- [ ] **Step 3:** Implementar `tools/compare-glb.py`: lê o chunk JSON e o binário, soma índices/3 de todas as primitivas de todas as malhas, aplica `matrix` ou `translation/rotation/scale` dos nós (percorrendo a árvore da cena) para a bounding box.
- [ ] **Step 4:** `bash tools/test-compare/run.sh` → todas as linhas `ok`.
- [ ] **Step 5:** Commit.

### Task 2: Script do ggml único

**Files:**
- Create: `scripts/ggml-unified.sh`
- Test: `tools/test-ggml/unified.sh`

**Interfaces:**
- Produces: `scripts/ggml-unified.sh <dest> <ps5sd-patches-dir>` monta o ggml em `<dest>` e escreve `<dest>/.paul3d-ggml` com uma linha por item aplicado (`leejet dc998771…`, `pwilkin <hash>` ×10, `ps5sd <arquivo>`). Se `<dest>/.paul3d-ggml` já existe com o mesmo conteúdo esperado, sai 0 sem tocar em nada. Em conflito: `git cherry-pick --abort`, mensagem `ggml-unified: pwilkin <hash> não aplica sobre leejet dc998771` e saída 1.

- [ ] **Step 1: Teste** — `tools/test-ggml/unified.sh` (num diretório temporário): roda o script → saída 0; `grep -c '^pwilkin ' .paul3d-ggml` = 10; `grep -c '^ps5sd ' .paul3d-ggml` = número de arquivos em `third_party/PS5SD/patches/ggml`; `grep 'GGML_VERSION_MINOR 27' CMakeLists.txt`; segunda execução → saída 0 e `git -C <dest> status --porcelain` sem mudanças novas; com um patch falso que conflita (copie os patches para um diretório temporário e acrescente um que altera a mesma linha já alterada) → saída 1 e a mensagem de conflito.
- [ ] **Step 2:** Rodar o teste → falha.
- [ ] **Step 3:** Implementar `scripts/ggml-unified.sh` (clone leejet, checkout fixo, `git fetch` pwilkin `f8697b33`, `git cherry-pick 456172ec..f8697b33`, `git apply` dos patches do PS5SD; identidade git local só para o cherry-pick).
- [ ] **Step 4:** Rodar o teste → `ok` em todas as linhas. Se algum commit do pwilkin conflitar de verdade: registrar qual no arquivo do plano e parar (decisão do usuário: pular commits só-CUDA, que não entram no build do PS5).
- [ ] **Step 5:** Commit.

### Task 3: Validação no PC (CPU)

**Files:**
- Create: `tools/test-ggml/compare-trellis.sh`

**Interfaces:**
- Consumes: `scripts/ggml-unified.sh` (Task 2), `tools/compare-glb.py` (Task 1).
- Produces: `tools/test-ggml/compare-trellis.sh <models-dir>` → monta dois trellis.cpp (`8e4ce11`, com `ps5/patch-*.py` exceto as linhas que dependem do PS5: a substituição por `paul3d_threads()` e `patch-trellis-io.py`), um com `pwilkin/ggml@f8697b33`, outro com o ggml único; roda `trellis-cli assets/goblin.png out.glb -m <models-dir> --gpu -1 --res 512 -s 42 --steps 4 --no-texture --faces 20000`; imprime os tempos por etapa dos dois e o resultado do comparador.

- [ ] **Step 1:** Escrever o script; os builds ficam em `${TMPDIR:-/tmp}/paul3d-ggml-check`, não no repositório.
- [ ] **Step 2:** Rodar com `~/Downloads/PS5MG-main/models/q4` → os dois `trellis-cli` compilam. Se o do ggml único não compilar: anotar o erro e as funções de ggml envolvidas na seção "Resultado" deste plano e parar.
- [ ] **Step 3:** Esperado: `PASS` do comparador; tempo total do ggml único até 10 % acima do atual. Colar a saída na seção "Resultado".
- [ ] **Step 4:** Commit do script e do resultado.

### Task 4: build-mg.sh usa o ggml único

**Files:**
- Modify: `scripts/build-mg.sh` (bloco que clona `pwilkin/ggml`, hoje as linhas com `git clone -q https://github.com/pwilkin/ggml "$G"`)

**Interfaces:**
- Consumes: `scripts/ggml-unified.sh "$G" "$P/patches/ggml"`.

- [ ] **Step 1:** Trocar o bloco `if [ ! -f "$G/CMakeLists.txt" ]; then … fi` por `bash "$W/scripts/ggml-unified.sh" "$G" "$P/patches/ggml"` (o script já é idempotente; os patches do PS5SD deixam de ser aplicados aqui).
- [ ] **Step 2:** `bash -n scripts/build-mg.sh` → sem erro.
- [ ] **Step 3:** Notas da 0.5.0 (só a base nova; resultado do 3D deve ser igual ao da 0.4.0; pedir o teste abaixo), `contentVersion` `01.000.010`.
- [ ] **Step 4:** Commit, tag `v0.5.0`, push; `gh run watch` → `title` e `release` com `success`. Se o build falhar, o log do passo "Build the title" vai para a seção "Resultado" e a tag é apagada (`git push --delete origin v0.5.0`) antes de corrigir.

### Task 5: Teste no console (com o usuário)

- [ ] **Step 1:** Instruções ao usuário: com a 0.4.0 instalada, gerar a foto `docs/images/icon-art.jpg` com seed 42, Balanced, textura desligada; baixar o GLB e o log (`tools/mg.sh log`). Instalar a 0.5.0 e repetir.
- [ ] **Step 2:** `python3 tools/compare-glb.py v040.glb v050.glb` → `PASS`; `python3 tools/runinfo.py` nos dois logs → tempo total da 0.5.0 até 10 % acima.
- [ ] **Step 3:** Se passar: marcar a fase 0 como concluída no spec e seguir para o plano da fase 1. Se não: resultado na seção "Resultado" e plano B do spec.

## Resultado

**Task 2 (2026-11-07):** ggml único = leejet `dc998771` + 2 patches do PS5SD. Os 10 commits do pwilkin ficam de fora: 9 só
mexem em `src/ggml-cuda/` (um deles, `0c672e3`, conflita com a 0.27) e `9dc0e51` corrige só Windows em ARM64 (conflita com o
backend Vulkan da 0.27). Nenhum entra no build do PS5 nem no teste em CPU.

**Task 3 (2026-11-07):** trellis.cpp `8e4ce11` **compila** contra o ggml único no PC (Linux, CPU), assim como contra o atual.
A geração em CPU foi interrompida: um passo da primeira etapa levou 469 s com 7 GB de RAM ocupados por outros programas, o que
dá várias horas por geração, vezes duas. A comparação de malha passa para o console (Task 5), que já estava no plano.

**Task 5 (2026-11-07, no console do usuário):** mesma foto, seed 42, 12 passos, 300k faces, atlas 2048, box UV.
Base antiga (job `w20261107-160342-0`, ggml pwilkin) × 0.5.0 (job `w20261107-192242-0`, ggml único):
`compare-glb.py` → **PASS**, faces 295406 / 295406, mesma bounding box; voxels idênticos em todas as etapas (880 @res32,
414144 @res512, remesh 982141). Tempo total **109,0 s → 103,8 s** (−5 %); flows 60,8/12,0/7,4 s → 55,5/10,8/6,6 s.
**Fase 0 concluída.**

