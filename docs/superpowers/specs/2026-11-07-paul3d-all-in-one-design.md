# PAUL3D tudo num só: texto, imagem e 3D dentro do mesmo app

Data: 2026-11-07. Estado: rascunho para aprovação. D1 decidida pelo usuário: download pelo próprio app.

## Intenção

Hoje, para ir de uma ideia até uma peça impressa, o usuário troca de app no PS5: PS5SD para fazer a imagem, PS5LM para a IA
de texto, PAUL3D para o 3D. Os três não rodam juntos no console. O objetivo é que o **PAUL3D faça tudo sozinho**, com os
modelos na pasta dele, controlado pela mesma página web:

- **Peça por texto**: uma frase → a IA de texto escreve OpenSCAD → o navegador gera o STL (a página `cad.html` já existe; hoje
  ela depende de PS5LM ou de um llama.cpp no PC).
- **Texto → imagem → 3D**: uma frase → imagem → malha 3D, sem trocar de app.
- **Imagem de referência + texto**: "a mesma figura, fundo branco, vista de 3/4" → imagem nova → 3D. Também serve para gerar a
  vista de trás que alimenta a "Foto de trás" (`--view`).

Quem usa: o dono do projeto, que imprime em 3D e controla o PS5 pela rede, de um PC ou do celular.

## Objetivos

1. Um só app no console (título `PPSA99577`), três motores: TRELLIS.2 (3D, já existe), llama.cpp (texto) e
   stable-diffusion.cpp (imagem).
2. Os modelos ficam em `/data/PAUL3D/models/`. Se já existirem nas pastas do PS5LM ou do PS5SD, o PAUL3D usa esses mesmos
   arquivos, sem copiar.
3. A página web ganha os fluxos acima, mantendo o que já existe (foto → 3D, comandos de impressão, foto de trás).
4. A `cad.html` passa a ter o próprio PAUL3D como servidor padrão da IA. A opção de um servidor externo (PS5LM ou PC)
   continua.

## Não-objetivos

- Chat geral, vídeo, LoRA, upscaler, inpainting, galeria completa do PS5SD. Só o que serve ao caminho até a peça.
- Rodar dois motores ao mesmo tempo no console.
- Treinar ou ajustar modelos.

## Anti-objetivos (falha mesmo que "funcione")

- Quebrar o que já funciona: foto → 3D tem que continuar com o mesmo tempo e a mesma qualidade (mesma seed, mesmo resultado).
- Deixar o console instável: mais reinícios do app por noite do que hoje, ou memória que acaba e trava o título.
- Um app que só funciona depois de baixar 20 GB: cada função tem que dizer claramente qual modelo falta e como colocar.

## Restrições conhecidas

- **Memória.** Pool da GPU de 11,4 GiB. No console, memória liberada por um processo não volta ao pool do mesmo processo
  (`mg_main.cpp`, PS5SD `docs/API.md` "There is no separate unload"). Por isso o PAUL3D já reinicia o título depois de cada job.
  Medidas do PS5SD: FLUX.2 Klein 4B Q8_0 + Qwen3-4B Q4_K_M = 7,1 GB, 1024×1024 em 4 passos em ~30 s. Do PS5LM:
  Qwen3.5 9B Q4_K_M = 5,3 GB, ~55 tokens/s. TRELLIS.2 q4 = 6,1 GB.
- **Reinícios.** Um job a mais no mesmo processo deixa o TRELLIS 4 a 9 vezes mais lento (causa não achada); depois de ~60
  reinícios numa noite o console já recusou abrir títulos uma vez (README). 80 seguidos passaram com 0.2.3.
- **Um app por vez.** PS5LM, PS5SD e PAUL3D não rodam juntos.
- **Download pelo app (D1, decidido).** O README hoje promete "the title has no network client and downloads nothing"; isso muda.
  A nova promessa: o título só acessa a internet quando o usuário clica em baixar um modelo, só fala com huggingface.co, e mostra
  a licença de cada modelo antes.
- **Versões do ggml.** O motor 3D usa `pwilkin/ggml` @ `f8697b3` = ggml upstream de 2026-09-14 (v0.24.0) + 10 commits.
  O stable-diffusion.cpp do PS5SD (`leejet/stable-diffusion.cpp` @ `b13a482`, 2026-10-18) traz um ggml de 2026-10-16. O PS5LM
  usa llama.cpp `b11827`. Duas cópias do ggml no mesmo executável não linkam (símbolos iguais).
- **Teste.** Só o console do usuário roda a versão PS5. Cada rodada é um build no GitHub Actions (~20 min) + instalação por FTP.
  No PC dá para validar no CPU (Linux, sem Vulkan instalado hoje).
- **Licenças.** O título é GPL-3.0. llama.cpp, stable-diffusion.cpp e ggml são MIT. FLUX.2 Klein 4B: Apache-2.0. Qwen3.5 9B e
  Qwen3-4B: Apache-2.0 (conferir no card ao fixar o arquivo). FLUX.2 Klein 9B tem licença própria (não comercial): fora.

## Arquitetura

### Um ggml para os três motores

Um único ggml, o mais novo dos dois: upstream na data do ggml do stable-diffusion.cpp fixado, mais os 10 commits do
`pwilkin/ggml` aplicados por cima, mais os 2 patches de ggml do PS5SD (`no-fork-backtrace`, `thread-affinity-and-priority`).
O llama.cpp é fixado num commit cuja sincronização de ggml seja a mesma data (o `src/` dele compilado contra esse ggml, sem a
cópia interna). Fica em `scripts/build-mg.sh`, como os patches atuais: clone fixado + scripts de patch idempotentes.

Antes de qualquer build para o PS5, validação no PC (CPU), com o mesmo ggml:
- TRELLIS.2: a mesma foto e seed dão a mesma malha que o ggml atual (comparar número de faces e bounding box; diferença numérica
  pequena aceita).
- llama.cpp: um GGUF pequeno (Qwen 0.5B–1.5B) responde.
- stable-diffusion.cpp: um modelo pequeno gera uma imagem.

### Um processo, um modo por vez (sessões)

O worker de `mg_main.cpp` hoje faz: pega um job, roda, reinicia o título. Ele passa a ter **modos**:

| Modo | Carrega | Atende | Termina (e reinicia o título) quando |
|---|---|---|---|
| `mesh` | TRELLIS.2 por job | um job de 3D | sempre, depois do job (como hoje) |
| `text` | o GGUF de texto, uma vez | pedidos de texto, um atrás do outro, com o modelo carregado | chega um job de outro tipo, ou 5 min sem pedido |
| `image` | o modelo de imagem, uma vez | pedidos de imagem em sequência | chega um job de outro tipo, ou 5 min sem pedido |

Assim a peça por texto (até 4 pedidos seguidos com correções) carrega o Qwen uma vez só, e nenhuma troca de motor acontece sem
reinício. O número de reinícios por noite fica igual ou menor que hoje para quem só faz 3D.

A fila continua em disco (`/data/PAUL3D/jobs`), com um campo de tipo no job. O servidor web continua no mesmo processo e fica
fora do ar alguns segundos a cada reinício (como hoje); a página já lida com isso porque consulta a fila.

### API (no servidor atual, porta 7870)

- `POST /api/text` — `{"messages":[...], "max_tokens", "temperature"}` → `{"id"}`. Resultado em `GET /api/text/<id>`:
  `{"status","text"}`, com o texto parcial enquanto gera (arquivo de saída lido pelo servidor). Formato das mensagens igual ao da
  OpenAI, para a `cad.html` usar os dois servidores com o mesmo código.
- `POST /v1/chat/completions` — compatibilidade OpenAI, **sem streaming de verdade**: segura a conexão até terminar quando o
  modelo já está carregado; responde `503` com `Retry-After` durante a troca de modo. Para clientes externos (aider, scripts).
- `POST /api/images` — `{"prompt","ref_image"?,"width","height","steps","seed"}` → `{"id"}`; `GET /api/images/<id>` →
  status e o PNG salvo em `/data/PAUL3D/in/<id>.png`. Esse arquivo pode virar um job de 3D direto (`from_image=<id>`), sem
  upload.
- `GET /api/models` ganha os modelos de texto e de imagem: quais existem, de qual pasta, quais faltam para cada função.

### Pastas de modelos

- Texto: `/data/PAUL3D/models/text/*.gguf`; se vazia, `/data/PS5LM/models/*.gguf`.
- Imagem: `/data/PAUL3D/models/image/` com um arquivo de preset no formato do PS5SD (difusão + VAE + encoder de texto); se
  vazia, os presets de `/data/PS5SD/presets` cujos arquivos existem.
- 3D: como hoje (`/data/PAUL3D/models/q4`).

### Download de modelos pelo app

- Código reaproveitado do PS5SD, que já é submódulo (`third_party/PS5SD/ps5/app/https_client.cpp`, `download_core.cpp`,
  `downloader.cpp`, `cacert.pem`, `scripts/build-mbedtls.sh`; GPL-3.0 como o PAUL3D): HTTPS com mbedTLS, retomada a partir do
  arquivo `.part`, 50 a 75 MB/s medidos no console.
- Um catálogo fixo no título (`catalog` com repositório, revisão, arquivo, tamanho e sha256, como `tools/weights.sha256`):
  conjunto 3D q4 (o mesmo do `setup-weights`), Qwen3.5 9B Q4_K_M, FLUX.2 Klein 4B Q8_0 + Qwen3-4B Q4_K_M + VAE.
  Só repositórios públicos do Hugging Face; sem token.
- Na página: lista "Modelos" com o que está instalado, o que falta por função, tamanho, licença com link, botão Baixar,
  progresso e Cancelar. Um modelo só baixa depois de o usuário marcar que leu a licença dele.
- Cada arquivo é conferido pelo sha256 antes de ser usado; um arquivo que não confere é apagado e a página diz.
- Baixar não roda junto com um job de 3D, texto ou imagem (rede e disco competem); fica na fila como um job de tipo `download`,
  sem reiniciar o título no fim.
- O `tools/setup-weights` do PC continua funcionando, para quem prefere.

### Página web

Três abas no topo do `index.html`: **Foto → 3D** (o que existe), **Texto → 3D** (prompt, referência opcional, imagem gerada com
"Gerar 3D desta imagem", "Gerar outra" e "Gerar vista de trás"), **Peça por texto** (a `cad.html`, com "PAUL3D (este console)"
como servidor padrão). Cada aba mostra, antes de tudo, se o modelo que ela precisa está instalado e, se não, como instalar.

### Modelos padrão

- Texto: Qwen3.5 9B Q4_K_M (5,3 GB), o mesmo da lista do PS5LM.
- Imagem: FLUX.2 Klein 4B Q8_0 + Qwen3-4B Q4_K_M + VAE (7,1 GB): faz imagem por texto e edição com referência.

## Fases e marcos

Cada fase termina com uma versão publicada que o usuário testa no console antes da próxima.

0. **ggml único** — build do PS5 com o ggml novo e só o 3D. Marco: v0.5.0, foto → 3D igual à 0.4.0 (mesma seed).
1. **Download + Texto** — download de modelos pelo app (catálogo, licenças, sha256), modo `text`, `POST /api/text`, `/v1/chat/completions`, `cad.html` usando o PAUL3D. Marco: v0.6.0, uma peça por
   texto gerada só com o PAUL3D aberto.
2. **Imagem** — modo `image`, `POST /api/images`, aba Texto → 3D. Marco: v0.7.0, prompt → imagem → malha sem trocar de app.
3. **Referência e vista de trás** — edição com referência e "Gerar vista de trás" ligada à Foto de trás. Marco: v0.8.0.

## Riscos

- **ggml novo muda o TRELLIS** (resultado ou velocidade). Mitigação: fase 0 isolada e comparada; se não der, plano B abaixo.
- **Plano B para versões de ggml**: compilar cada motor com seu ggml e renomear os símbolos de um deles no link (prefixo). Mais
  frágil; só se o A falhar.
- **Memória do modo texto ou imagem acima do medido** (o PAUL3D também tem o servidor web e a tela). Mitigação: os números do
  PS5LM/PS5SD são do mesmo console; o modo carrega um modelo por vez e reinicia para trocar.
- **Rodadas de teste longas.** Mitigação: tudo que dá para validar no PC é validado antes (CPU), e cada versão só muda uma fase.
- **Título maior** (três motores + mbedTLS): o `eboot.bin` cresce. Aceitável; medir.
- **Download corrompido ou parcial** num disco de console. Mitigação: `.part` + sha256 + nada é usado antes de conferir.

## Decisões do usuário

- **D1 — Como os modelos chegam ao console: pelo próprio app** (botão Baixar na página), como no PS5SD e no PS5LM.

## Decisões tomadas por mim (Claude), para o usuário conferir

- Texto → 3D **não** encadeia sozinho: a imagem aparece e o 3D só começa com "Gerar 3D desta imagem". Uma malha leva 1 a 3 min;
  gastar isso numa imagem ruim é pior que um clique.
- Modos `text` e `image` mantêm o modelo carregado entre pedidos; o modo `mesh` continua reiniciando a cada job.
- A versão atual (página Peça por texto, logo novo) sai antes como v0.4.0, base de comparação da fase 0.
- Reusar as pastas do PS5LM e do PS5SD quando os modelos já estão lá, em vez de pedir cópia.

## Deixado para quem constrói

- O commit exato do llama.cpp e do stable-diffusion.cpp, desde que o ggml seja um só e a validação da fase 0 passe.
- Quais patches do PS5LM (`patches/0001`, `0002`) e do PS5SD (`patches/sdcpp/0001-0003`, `0067`, `0071`, `0073`) entram; os
  de servidor HTTP deles não entram (o servidor é o do PAUL3D).
- Formato exato dos arquivos de job e de preset de imagem.
- O tempo de 5 min sem pedido para encerrar um modo.
