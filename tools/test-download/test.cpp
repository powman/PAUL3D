// Tests of ps5/mg_download (catalogue, state on disk, download with resume and sha256) on the PC.
//   test <root-of-repo> <port> <file.bin> <sha256-of-file> <server-log>
#include "mg_download.hpp"
#include "httplib.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

static int fails = 0;
static void t(const char * name, bool ok, const std::string & d = "") {
    std::printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, d.empty() ? "" : ": ", d.c_str());
    if (!ok) fails++;
}
static std::string slurp(const std::string & p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char ** argv) {
    namespace fs = std::filesystem;
    const std::string root = argv[1], file = argv[3], sha = argv[4], log = argv[5];
    const int port = std::atoi(argv[2]);
    const int64_t size = (int64_t) fs::file_size(file);

    std::string err;
    auto cat = mg::parse_catalog(slurp(root + "/ps5/models.txt"), err);
    t("catalog: the text model only", cat.size() == 1 && cat[0].group == "text-qwen35-9b", err);
    t("catalog: the Qwen line", !cat.empty() && cat.back().sha256 == "03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8" &&
      cat.back().size == 5680522464LL && cat.back().dest == "models/text/Qwen3.5-9B-Q4_K_M.gguf");
    t("catalog: hf_url", !cat.empty() && mg::hf_url(cat.back()) ==
      "https://huggingface.co/unsloth/Qwen3.5-9B-GGUF/resolve/3885219b6810b007914f3a7950a8d1b469d598a5/Qwen3.5-9B-Q4_K_M.gguf");
    auto bad = mg::parse_catalog("# c\nmesh-q4 abc 1 r v p\n", err);
    t("catalog: a short line is an error with its number", bad.empty() && err.find("line 2") != std::string::npos, err);

    t("sha256 of empty", mg::sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    t("sha256 of abc", mg::sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    fs::path d = fs::temp_directory_path() / "paul3d-dl"; fs::remove_all(d); fs::create_directories(d);
    mg::ModelFile f{"g", sha, "repo", "rev", "file.bin", "models/x/file.bin", "lic", size};
    auto st = mg::catalog_state({f}, d.string());
    t("state: nothing yet", st.size() == 1 && st[0].have == 0 && !st[0].installed);

    std::vector<std::string> ranges;
    mg::Fetch fetch = [&](const std::string & url, int64_t from, const std::function<bool(const char *, size_t)> & sink,
                          int64_t & total, std::string & e) {
        httplib::Client c("127.0.0.1", port);
        httplib::Headers h; if (from > 0) h.emplace("Range", "bytes=" + std::to_string(from) + "-");
        auto r = c.Get(url.substr(url.find('/', 8)), h, [&](const char * p, size_t n) { return sink(p, n); });
        if (!r) { e = "connection lost"; return false; }
        if (r->status != 200 && r->status != 206) { e = "HTTP " + std::to_string(r->status); return false; }
        total = from + (int64_t) std::stoll(r->get_header_value("Content-Length"));
        return true;
    };
    const int64_t lots = 1LL << 40;
    const std::string base = "http://127.0.0.1:" + std::to_string(port);
    bool ok = mg::download_file(f, d.string(), base + "/cut/file.bin", fetch, lots, [](int64_t, int64_t) { return true; }, err);
    t("cut: the first try fails", !ok, err);
    st = mg::catalog_state({f}, d.string());
    t("cut: a .part is kept", fs::exists(d / "models/x/file.bin.part") && st[0].have > 0 && st[0].have < size && !st[0].installed);
    ok = mg::download_file(f, d.string(), base + "/cut/file.bin", fetch, lots, [](int64_t, int64_t) { return true; }, err);
    t("resume: the second try finishes, no error left", ok && err.empty(), err);
    const std::string l = slurp(log);
    t("resume: it asked for the rest only", l.find("bytes=" + std::to_string(st[0].have) + "-") != std::string::npos, l);
    t("resume: the file is in place, no .part", fs::exists(d / "models/x/file.bin") && !fs::exists(d / "models/x/file.bin.part"));
    t("resume: right bytes", mg::sha256_hex(slurp((d / "models/x/file.bin").string())) == sha);
    t("state: installed", mg::catalog_state({f}, d.string())[0].installed);

    fs::remove_all(d); fs::create_directories(d);
    ok = mg::download_file(f, d.string(), base + "/bad/file.bin", fetch, lots, [](int64_t, int64_t) { return true; }, err);
    t("bad bytes: fails naming sha256", !ok && err.find("sha256") != std::string::npos, err);
    t("bad bytes: nothing left", !fs::exists(d / "models/x/file.bin") && !fs::exists(d / "models/x/file.bin.part"));

    int calls = 0;
    mg::Fetch counting = [&](const std::string & u, int64_t from, const std::function<bool(const char *, size_t)> & s, int64_t & tot, std::string & e) {
        calls++; return fetch(u, from, s, tot, e); };
    ok = mg::download_file(f, d.string(), base + "/ok/file.bin", counting, size / 2, [](int64_t, int64_t) { return true; }, err);
    t("no space: refused before connecting", !ok && calls == 0 && err.find("GB") != std::string::npos, err);

    ok = mg::download_file(f, d.string(), base + "/ok/file.bin", fetch, lots, [](int64_t done, int64_t) { return done < 1000; }, err);
    t("cancel: the progress callback stops it", !ok && err.find("cancel") != std::string::npos, err);
    fs::remove_all(d);
    return fails ? 1 : 0;
}
