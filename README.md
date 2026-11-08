<p align="center"><img src="docs/images/logo.png" width="160" alt="PAUL3D logo"></p>

# PAUL3D: parts to print, from a sentence, on a PS5

Write what you want to print ("a box 60 × 40 × 25 mm with a snap-on lid, 2 mm walls"). A language model running on the
console's GPU writes an [OpenSCAD](https://openscad.org) program, your browser compiles it and shows the part, ready as STL.
Then write what to change ("holes 1 mm larger"). Exact measurements, for functional parts: boxes, brackets, hooks, key rings,
spare parts.

> PAUL3D is a modified version of [PS5MG](https://github.com/nikay99/PS5MG) by nikay99 (GPL-3.0). Since 0.7.0 it has only
> the text-to-part path; the picture-to-mesh engine of PS5MG (TRELLIS.2) is gone. PAUL3D is not affiliated with Sony,
> ships no exploit and asks for none.

## What you need

- A PS5 that runs homebrew titles (ShadowMountPlus) and an FTP server on port 2121.
- A text model (GGUF) on the console: the page downloads Qwen3.5 9B (5.7 GB, Apache-2.0) for you, or copy any `.gguf` that
  llama.cpp reads to `/data/PAUL3D/models/text` (models in PS5LM's `/data/PS5LM/models` count too).
- A browser on the same network, with internet once (three.js, OpenSCAD as WebAssembly, a font, from jsDelivr).

## Install

1. Download `PPSA99577.zip` from the [latest release](https://github.com/powman/PAUL3D/releases/latest), unzip it and copy the
   folder `PPSA99577` to `/data/homebrew` with any FTP client (port 2121). Rescan in ShadowMountPlus and start PAUL3D.
2. Open `http://<console>:7870`. Under **Modelos**, tick that you read the licence and press **Baixar**; or copy a `.gguf`.
3. Describe a part with its measurements in mm and press **Gerar peça**.

## How it runs

- **One model, loaded once.** The title loads the text model when it starts (the one you used last) and keeps it in the GPU
  for as long as it runs: only the first answer after a start waits for the load (Qwen3.5 9B about 40 s, Qwen3.8 27B about
  2 minutes). Choosing another model under **Modelo** makes the title start over with that one.
- **Room for a big model.** 4096 tokens of context and a batch of 512 keep the compute buffers small, so a 27B model in 2-bit
  (9.8 GB of the 11.4 GiB pool) fits. A part's request, its answer and up to three fixes fit in 4096 tokens.
- **Measured on a console:** Qwen3.8 27B UD-Q2_K_XL answers at 16 to 18 tokens a second, about 35 s for a part.
- **Errors go back to the model.** When OpenSCAD reports an error, the page sends it back, up to three times. The code stays
  editable, and **Compilar** runs it again without the model.
- **Downloads on the console** (Modelos): only from huggingface.co, only the pinned file of `ps5/models.txt`, checked by
  sha256 before it is used; an interrupted download goes on from where it stopped.
- **The TV** shows the address, the model, what the console is doing and the log.

## For other programs

The same model answers OpenAI's API: `GET /v1/models`, `POST /v1/chat/completions` (the answer comes when it is done). All
routes: [docs/API.md](docs/API.md). An optional key protects the page and the API: `tools/mg.sh setkey <key>`.

## Build

[docs/BUILDING.md](docs/BUILDING.md). Tests that run on a PC: `tools/test-*` (the page's logic, the text engine with a small
Qwen3.5 on the CPU, the queue, the downloads, the web server in a container, the unified ggml).

## Licences

PAUL3D is GPL-3.0. It builds llama.cpp, ggml and cpp-httplib (MIT) and PS5SD's console code (GPL-3.0); see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No model weights are in this repository or in the title: each model has its
own licence, shown before it is downloaded. PlayStation and PS5 are trademarks of Sony Interactive Entertainment.
