// The smallest use of llama.cpp's API that PAUL3D needs: load a GGUF without mmap, apply the model's own chat
// template, answer greedily. Prints "arch=<architecture>" and the answer.   usage: hello <model.gguf> "<question>"
#include "llama.h"
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char ** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: hello <model.gguf> <question>\n"); return 2; }
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.load_mode = LLAMA_LOAD_MODE_NONE;   // read the file (no mmap), as the console title will
    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { std::fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    char arch[64] = {0};
    llama_model_meta_val_str(model, "general.architecture", arch, sizeof arch);
    std::printf("arch=%s\n", arch);

    llama_chat_message msg{"user", argv[2]};
    std::vector<char> buf(8192);
    const char * tmpl = llama_model_chat_template(model, nullptr);
    int n = llama_chat_apply_template(tmpl, &msg, 1, true, buf.data(), (int) buf.size());
    if (n < 0) { std::fprintf(stderr, "chat template failed\n"); return 1; }
    std::string prompt(buf.data(), n);
    if (tmpl && std::string(tmpl).find("<think>") != std::string::npos) prompt += "<think>\n\n</think>\n\n";

    const llama_vocab * vocab = llama_model_get_vocab(model);
    std::vector<llama_token> toks(prompt.size() + 8);
    int nt = llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), toks.data(), (int) toks.size(), true, true);
    toks.resize(nt);
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 2048;
    llama_context * ctx = llama_init_from_model(model, cp);
    llama_sampler * smp = llama_sampler_init_greedy();
    llama_batch batch = llama_batch_get_one(toks.data(), (int) toks.size());
    std::string out;
    for (int i = 0; i < 32; i++) {
        if (llama_decode(ctx, batch)) { std::fprintf(stderr, "decode failed\n"); return 1; }
        llama_token t = llama_sampler_sample(smp, ctx, -1);
        if (llama_vocab_is_eog(vocab, t)) break;
        char piece[256];
        int pn = llama_token_to_piece(vocab, t, piece, sizeof piece, 0, true);
        out.append(piece, pn > 0 ? pn : 0);
        batch = llama_batch_get_one(&t, 1);
    }
    std::printf("answer=%s\n", out.c_str());
    llama_sampler_free(smp);
    llama_free(ctx);
    llama_model_free(model);
    return 0;
}
