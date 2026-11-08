# Third-party notices

PAUL3D's own code is GPL-3.0 (`LICENSE`). The title also contains, or is built with, the following. Each keeps its own
licence, and the licence texts that ship with their sources are copied into `licenses/` of the title.

## In the title

| Component | What for | Copyright | Licence |
|---|---|---|---|
| [PS5MG](https://github.com/nikay99/PS5MG) | the project PAUL3D is a modified version of | nikay99 | GPL-3.0 |
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | the text engine (its `src/`, and from its `vendor/`: nlohmann/json, stb_image) | The llama.cpp authors | MIT |
| [ggml](https://github.com/leejet/ggml) (leejet's fork of ggml-org/ggml, as stable-diffusion.cpp pins it) | tensor library, Vulkan backend | The ggml authors | MIT |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | the web server | Yuji Hirose | MIT |
| [PS5SD](https://github.com/Darkatek7/PS5SD) and [PS5LM](https://github.com/cobanov/PS5LM) code | title start-up, screen, build recipe | their authors | GPL-3.0 |
| [ps5-homebrew-ui](https://github.com/Phononenom/ps5-homebrew-ui) | the dashboard on the screen | BlackBearReloaded and contributors | GPL-3.0-or-later (its own notices: `licenses/ps5-homebrew-ui-THIRD_PARTY_NOTICES.md`) |
| Inter, Montserrat, DejaVu Sans Mono | the dashboard's fonts | their authors | SIL OFL 1.1 / Bitstream Vera (texts in `assets/fonts/`) |
| [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan), PS5_Mesa (RADV), PS5_PayloadSDK | Vulkan on the console, the toolchain | mihawk-99 and the Mesa authors | see those repositories (Mesa: MIT) |
| three.js | the viewer in the web page (loaded by your browser from a CDN, not shipped) | three.js authors | MIT |
| [openscad-wasm](https://github.com/lorenzowritescode/openscad-wasm) | OpenSCAD in the web page (loaded by your browser from a CDN, not shipped) | OpenSCAD authors | GPL-2.0-or-later |

## Not in the title, not in this repository

The model weights. The page's Modelos panel downloads Qwen3.5 9B (Qwen team, Apache-2.0) from Hugging Face after you tick that
you read its licence; any other GGUF you copy yourself comes under its own licence.

PAUL3D is not affiliated with or endorsed by Sony Interactive Entertainment. "PlayStation" and "PS5" are trademarks of Sony
Interactive Entertainment, named here only to say what PAUL3D runs on.
