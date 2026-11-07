// The text engine of PAUL3D: a GGUF language model (llama.cpp on the unified ggml) answering chat messages.
// Used by the worker's text mode (mg_main.cpp) for "Peça por texto"; tested on a PC by tools/test-llama.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct llama_model;
struct llama_context;

namespace mg {

struct ChatMsg {
    std::string role;     // system | user | assistant
    std::string content;
};

class TextEngine {
  public:
    TextEngine();
    ~TextEngine();
    TextEngine(const TextEngine &) = delete;
    TextEngine & operator=(const TextEngine &) = delete;

    // Reads the whole model (no mmap). n_ctx: tokens of prompt + answer.
    bool load(const std::string & gguf, int n_ctx, int n_threads, std::string & err);
    bool loaded() const { return ctx_ != nullptr; }
    std::string model_name() const { return name_; }

    // One answer with the model's own chat template, thinking (<think>) turned off. on_piece gets each piece of text;
    // returning false stops the answer there (still a success). temperature 0 is greedy.
    bool chat(const std::vector<ChatMsg> & msgs, int max_tokens, float temperature,
              const std::function<bool(const std::string &)> & on_piece, std::string & out, std::string & err);

  private:
    llama_model *   model_ = nullptr;
    llama_context * ctx_   = nullptr;
    std::string     name_, tmpl_;
    int             n_ctx_ = 0;
};

}  // namespace mg
