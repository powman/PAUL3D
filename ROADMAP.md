# Roadmap

PAUL3D makes parts to print from a sentence, on the console, with nothing else to run. Earlier versions (0.1 to 0.6) also
turned a picture into a 3D mesh with TRELLIS.2 (the PS5MG engine); 0.7.0 keeps only the text path.

## Done
- **0.4.0:** a part from a sentence: a language model writes OpenSCAD, the browser compiles it.
- **0.5.0:** one ggml for the engines (leejet 0.27 + PS5SD's patches).
- **0.6.0:** the text model runs in the title; models are downloaded from the page (licence, sha256, resume).
- **0.6.1:** choose the text model; wait 5 minutes for a big model to load.
- **0.7.0:** text only: the 3D engine is gone, the text model is loaded at start and stays, 4096-token context for a 27B model.

## Next
- Download speed: 3 MB/s on the console against 50 to 75 MB/s in PS5SD (which writes through ftpsrv).
- More models in the Modelos catalogue (Qwen3.8 27B), with their memory needs.
