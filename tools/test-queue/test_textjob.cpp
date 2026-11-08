// Tests of ps5/mg_textjob: the text request's JSON, the files the worker writes, the status the web server answers.
#include "mg_textjob.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int fails = 0;
static void t(const char * name, bool ok, const std::string & detail = "") {
    std::printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, detail.empty() ? "" : ": ", detail.c_str());
    if (!ok) fails++;
}

int main() {
    namespace fs = std::filesystem;
    mg::TextJob j; std::string err;
    t("valid request", mg::parse_text_job(R"({"messages":[{"role":"system","content":"s"},{"role":"user","content":"u"}],"max_tokens":100,"temperature":0.5})", j, err), err);
    t("fields read", j.messages.size() == 2 && j.messages[1].content == "u" && j.max_tokens == 100 && j.temperature == 0.5f);
    t("defaults", mg::parse_text_job(R"({"messages":[{"role":"user","content":"u"}]})", j, err) && j.max_tokens == 4096 && j.temperature == 0.2f);
    t("not JSON", !mg::parse_text_job("{nope", j, err) && err.find("JSON") != std::string::npos, err);
    t("no messages", !mg::parse_text_job(R"({"messages":[]})", j, err) && err.find("messages") != std::string::npos, err);
    t("bad role", !mg::parse_text_job(R"({"messages":[{"role":"robot","content":"u"}]})", j, err) && err.find("role") != std::string::npos, err);
    std::string big(200001, 'a');
    t("too long", !mg::parse_text_job(R"({"messages":[{"role":"user","content":")" + big + R"("}]})", j, err) && err.find("200000") != std::string::npos, err);
    t("max_tokens clamped", mg::parse_text_job(R"({"messages":[{"role":"user","content":"u"}],"max_tokens":999999})", j, err) && j.max_tokens == 8192);

    t("model field read", mg::parse_text_job(R"({"messages":[{"role":"user","content":"u"}],"model":"Qwen3.8-27B.gguf"})", j, err) && j.model == "Qwen3.8-27B.gguf", err);
    t("no model field: empty", mg::parse_text_job(R"({"messages":[{"role":"user","content":"u"}]})", j, err) && j.model.empty());
    t("model with a path is refused", !mg::parse_text_job(R"({"messages":[{"role":"user","content":"u"}],"model":"../x.gguf"})", j, err) && err.find("model") != std::string::npos, err);

    {   // installed text models: both folders, by name, PAUL3D's own first when a name is in both
        fs::path m = fs::temp_directory_path() / "paul3d-models"; fs::remove_all(m);
        fs::create_directories(m / "own"); fs::create_directories(m / "ps5lm");
        std::ofstream(m / "own" / "b.gguf") << "x"; std::ofstream(m / "ps5lm" / "a.gguf") << "x"; std::ofstream(m / "ps5lm" / "b.gguf") << "x";
        std::ofstream(m / "own" / "notes.txt") << "x";
        const std::vector<std::string> dirs{(m / "own").string(), (m / "ps5lm").string()};
        auto list = mg::list_text_models(dirs);
        t("list: two names, sorted", list.size() == 2 && list[0].name == "a.gguf" && list[1].name == "b.gguf");
        t("list: own folder wins a shared name", list.size() == 2 && list[1].path == (m / "own" / "b.gguf").string());
        t("resolve: a listed name", mg::resolve_text_model(dirs, "a.gguf") == (m / "ps5lm" / "a.gguf").string());
        t("resolve: empty name is the default", mg::resolve_text_model(dirs, "") == mg::find_text_model(dirs));
        t("resolve: unknown name", mg::resolve_text_model(dirs, "zzz.gguf").empty());
        mg::TextJob want; want.model = "a.gguf";
        t("other model: a different one is asked", mg::needs_other_model(want, (m / "own" / "b.gguf").string(), dirs));
        want.model = "b.gguf";
        t("other model: the loaded one", !mg::needs_other_model(want, (m / "own" / "b.gguf").string(), dirs));
        want.model = "";
        t("other model: no name keeps the loaded one", !mg::needs_other_model(want, (m / "ps5lm" / "a.gguf").string(), dirs));
        // the model the title loads when it starts: the last one asked for, while it is installed
        const std::string pref = (m / "text-model.txt").string();
        t("preferred: none written, the default", mg::preferred_text_model(dirs, pref) == mg::find_text_model(dirs));
        mg::remember_text_model(pref, "b.gguf");
        t("preferred: the remembered one", mg::preferred_text_model(dirs, pref) == (m / "own" / "b.gguf").string());
        mg::remember_text_model(pref, "gone.gguf");
        t("preferred: a removed one falls back", mg::preferred_text_model(dirs, pref) == mg::find_text_model(dirs));
        fs::remove_all(m);
    }

    fs::path d = fs::temp_directory_path() / "paul3d-textjob"; fs::remove_all(d);
    const std::string jobs = (d / "jobs").string(), text = (d / "text").string();
    fs::create_directories(jobs); fs::create_directories(text);
    t("unknown id", mg::text_status(jobs, text, "t1") == "");
    std::ofstream(d / "jobs" / "t1.textjob") << "{}";
    t("queued", mg::text_status(jobs, text, "t1").find(R"("status":"queued")") != std::string::npos, mg::text_status(jobs, text, "t1"));
    fs::rename(d / "jobs" / "t1.textjob", d / "jobs" / "t1.textrunning");
    t("loading", mg::text_status(jobs, text, "t1").find(R"("status":"loading")") != std::string::npos, mg::text_status(jobs, text, "t1"));
    mg::write_text_partial(text, "t1", "cube(\"10\");");
    std::string s = mg::text_status(jobs, text, "t1");
    t("generating with the text, escaped", s.find(R"("status":"generating")") != std::string::npos && s.find(R"(cube(\"10\");)") != std::string::npos, s);
    mg::write_text_done(text, "t1", "done", "");
    t("done", mg::text_status(jobs, text, "t1").find(R"("status":"done")") != std::string::npos, mg::text_status(jobs, text, "t1"));
    mg::write_text_done(text, "t2", "failed", "missing_model");
    s = mg::text_status(jobs, text, "t2");
    t("failed with the error", s.find(R"("status":"failed")") != std::string::npos && s.find(R"("error":"missing_model")") != std::string::npos, s);
    t("no partial temp files left", !fs::exists(d / "text" / "t1.txt.tmp"));
    fs::remove_all(d);
    return fails ? 1 : 0;
}
