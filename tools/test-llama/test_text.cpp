// Tests of ps5/mg_text (the console's text engine) on the PC, with a small Qwen3.5.   usage: test_text <model.gguf>
#include "mg_text.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int fails = 0;
static void t(const char * name, bool ok, const std::string & detail = "") {
    std::printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, detail.empty() ? "" : ": ", detail.c_str());
    if (!ok) fails++;
}

int main(int argc, char ** argv) {
    if (argc < 2) return 2;
    namespace fs = std::filesystem;
    mg::TextEngine e;
    std::string err;
    t("not loaded before load", !e.loaded());
    t("load", e.load(argv[1], 4096, 4, err), err);
    t("loaded", e.loaded());
    t("model_name is the file name", e.model_name() == fs::path(argv[1]).filename().string(), e.model_name());

    std::string out;
    std::vector<mg::ChatMsg> ask{{"user", "Reply with exactly the word OK."}};
    std::string joined;
    bool ok = e.chat(ask, 32, 0.0f, [&](const std::string & p) { joined += p; return true; }, out, err);
    t("chat_ok", ok && out.find("OK") != std::string::npos, ok ? out : err);
    t("pieces_join_to_out", joined == out);
    t("no_think_block", out.find("<think>") == std::string::npos && out.find("</think>") == std::string::npos, out);

    int n = 0; std::string part;
    ok = e.chat({{"user", "Count from 1 to 50, separated by spaces."}}, 200, 0.0f,
                [&](const std::string & p) { part += p; return ++n < 3; }, out, err);
    t("stop_early", ok && out == part && n == 3, out);

    mg::TextEngine small;
    small.load(argv[1], 512, 4, err);
    std::string longmsg; for (int i = 0; i < 20000; i++) longmsg += "word ";
    ok = small.chat({{"user", longmsg}}, 16, 0.0f, [](const std::string &) { return true; }, out, err);
    t("prompt_too_long", !ok && err.find("context") != std::string::npos, err);

    fs::path d = fs::temp_directory_path() / "paul3d-find-model";
    fs::remove_all(d); fs::create_directories(d / "one"); fs::create_directories(d / "two");
    std::ofstream(d / "two" / "b.gguf") << "x"; std::ofstream(d / "two" / "a.gguf") << "x"; std::ofstream(d / "two" / "x.txt") << "x";
    std::string found = mg::find_text_model({(d / "one").string(), (d / "two").string()});
    t("find_text_model picks a.gguf", found == (d / "two" / "a.gguf").string(), found);
    t("find_text_model none", mg::find_text_model({(d / "one").string(), (d / "missing").string()}).empty());
    fs::remove_all(d);
    return fails ? 1 : 0;
}
