#include "mg_text.hpp"

#include "llama.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>

namespace mg {

TextEngine::TextEngine() = default;

TextEngine::~TextEngine() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_model_free(model_);
}

bool TextEngine::load(const std::string & gguf, int n_ctx, int n_threads, std::string & err) {
    static bool backend = (llama_backend_init(), true);
    (void) backend;
    llama_model_params mp = llama_model_default_params();
    mp.load_mode = LLAMA_LOAD_MODE_NONE;  // plain reads: big stdio buffers (ps5/patch-llama-io.py), no mmap in a title
    model_ = llama_model_load_from_file(gguf.c_str(), mp);
    if (!model_) { err = "cannot load " + gguf; return false; }
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx           = (uint32_t) n_ctx;
    cp.n_batch         = (uint32_t) std::min(n_ctx, 2048);
    cp.n_threads       = n_threads;
    cp.n_threads_batch = n_threads;
    ctx_ = llama_init_from_model(model_, cp);
    if (!ctx_) { err = "cannot make a context of " + std::to_string(n_ctx) + " tokens"; return false; }
    n_ctx_ = n_ctx;
    name_  = std::filesystem::path(gguf).filename().string();
    const char * t = llama_model_chat_template(model_, nullptr);
    tmpl_ = t ? t : "";
    return true;
}

bool TextEngine::chat(const std::vector<ChatMsg> & msgs, int max_tokens, float temperature,
                      const std::function<bool(const std::string &)> & on_piece, std::string & out, std::string & err) {
    out.clear();
    if (!ctx_) { err = "no model loaded"; return false; }
    std::vector<llama_chat_message> cm;
    for (const auto & m : msgs) cm.push_back({m.role.c_str(), m.content.c_str()});
    const char * tmpl = tmpl_.empty() ? nullptr : tmpl_.c_str();
    std::vector<char> buf(4096);
    int n = llama_chat_apply_template(tmpl, cm.data(), cm.size(), true, buf.data(), (int) buf.size());
    if (n > (int) buf.size()) { buf.resize(n); n = llama_chat_apply_template(tmpl, cm.data(), cm.size(), true, buf.data(), n); }
    if (n < 0) { err = "the model's chat template cannot be applied"; return false; }
    std::string prompt(buf.data(), n);
    if (tmpl_.find("<think>") != std::string::npos) prompt += "<think>\n\n</think>\n\n";  // thinking off

    const llama_vocab * vocab = llama_model_get_vocab(model_);
    std::vector<llama_token> toks(prompt.size() + 16);
    int nt = llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), toks.data(), (int) toks.size(), true, true);
    if (nt < 0) { toks.resize(-nt); nt = llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), toks.data(), (int) toks.size(), true, true); }
    toks.resize(std::max(nt, 0));
    if ((int) toks.size() >= n_ctx_) {
        err = "the prompt is longer than the context (" + std::to_string(toks.size()) + " tokens, context " + std::to_string(n_ctx_) + ")";
        return false;
    }
    max_tokens = std::min(max_tokens, n_ctx_ - (int) toks.size());

    llama_memory_clear(llama_get_memory(ctx_), true);
    const int n_batch = (int) llama_n_batch(ctx_);
    for (size_t i = 0; i < toks.size(); i += n_batch) {
        const int len = (int) std::min<size_t>(n_batch, toks.size() - i);
        if (llama_decode(ctx_, llama_batch_get_one(toks.data() + i, len))) { err = "the model failed on the prompt"; return false; }
    }

    llama_sampler * smp = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (temperature <= 0.0f) {
        llama_sampler_chain_add(smp, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(smp, llama_sampler_init_top_p(0.95f, 1));
        llama_sampler_chain_add(smp, llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(smp, llama_sampler_init_dist(std::random_device{}()));
    }
    bool ok = true;
    for (int i = 0; i < max_tokens; i++) {
        llama_token t = llama_sampler_sample(smp, ctx_, -1);
        if (llama_vocab_is_eog(vocab, t)) break;
        char piece[256];
        const int pn = llama_token_to_piece(vocab, t, piece, sizeof piece, 0, false);
        if (pn > 0) {
            std::string p(piece, pn);
            out += p;
            if (!on_piece(p)) break;
        }
        if (llama_decode(ctx_, llama_batch_get_one(&t, 1))) { err = "the model failed while answering"; ok = false; break; }
    }
    llama_sampler_free(smp);
    return ok;
}

std::string find_text_model(const std::vector<std::string> & dirs) {
    namespace fs = std::filesystem;
    for (const auto & d : dirs) {
        std::error_code ec;
        std::vector<std::string> names;
        for (const auto & e : fs::directory_iterator(d, ec)) {
            if (e.is_regular_file(ec) && e.path().extension() == ".gguf") names.push_back(e.path().string());
        }
        if (!names.empty()) { std::sort(names.begin(), names.end()); return names.front(); }
    }
    return "";
}

}  // namespace mg
