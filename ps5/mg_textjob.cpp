#include "mg_textjob.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

#include "nlohmann/json.hpp"

namespace mg {

using json = nlohmann::json;

static bool exists(const std::string & p) { struct stat st; return stat(p.c_str(), &st) == 0; }
static std::string slurp(const std::string & p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

bool parse_text_job(const std::string & text, TextJob & out, std::string & err) {
    out = TextJob{};
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { err = "the request is not JSON"; return false; }
    if (!j.contains("messages") || !j["messages"].is_array() || j["messages"].empty()) { err = "messages is empty"; return false; }
    size_t chars = 0;
    for (const auto & m : j["messages"]) {
        if (!m.is_object() || !m.contains("role") || !m["role"].is_string() || !m.contains("content") || !m["content"].is_string()) {
            err = "every message needs a role and a content"; return false;
        }
        const std::string role = m["role"];
        if (role != "system" && role != "user" && role != "assistant") { err = "role is system, user or assistant"; return false; }
        out.messages.push_back({role, m["content"].get<std::string>()});
        chars += out.messages.back().content.size();
    }
    if (chars > kTextJobMaxChars) { err = "the messages are longer than 200000 characters"; return false; }
    if (j.contains("max_tokens") && j["max_tokens"].is_number()) out.max_tokens = std::clamp(j["max_tokens"].get<int>(), 1, 8192);
    if (j.contains("model") && j["model"].is_string()) {
        out.model = j["model"];
        if (out.model.find('/') != std::string::npos || out.model.find("..") != std::string::npos) { err = "model is a file name"; return false; }
    }
    if (j.contains("temperature") && j["temperature"].is_number()) out.temperature = std::clamp(j["temperature"].get<float>(), 0.0f, 2.0f);
    return true;
}

void write_text_partial(const std::string & dir, const std::string & id, const std::string & text) {
    const std::string p = dir + "/" + id + ".txt";
    { std::ofstream f(p + ".tmp", std::ios::binary); f << text; }
    std::rename((p + ".tmp").c_str(), p.c_str());
}

void write_text_done(const std::string & dir, const std::string & id, const std::string & status, const std::string & error) {
    json j = {{"status", status}};
    if (!error.empty()) j["error"] = error;
    const std::string p = dir + "/" + id + ".textdone";
    { std::ofstream f(p + ".tmp"); f << j.dump(); }
    std::rename((p + ".tmp").c_str(), p.c_str());
}

std::string find_text_model(const std::vector<std::string> & dirs) {
    std::vector<std::string> names;
    for (const auto & d : dirs) {
        names.clear();
        if (DIR * h = opendir(d.c_str())) {
            while (dirent * e = readdir(h)) {
                const std::string n = e->d_name;
                struct stat st;
                if (n.size() > 5 && n.compare(n.size() - 5, 5, ".gguf") == 0 && stat((d + "/" + n).c_str(), &st) == 0 && S_ISREG(st.st_mode))
                    names.push_back(d + "/" + n);
            }
            closedir(h);
        }
        if (!names.empty()) { std::sort(names.begin(), names.end()); return names.front(); }
    }
    return "";
}

std::vector<TextModel> list_text_models(const std::vector<std::string> & dirs) {
    std::vector<TextModel> out;
    for (const auto & d : dirs) {
        if (DIR * h = opendir(d.c_str())) {
            while (dirent * e = readdir(h)) {
                const std::string n = e->d_name;
                struct stat st;
                if (n.size() <= 5 || n.compare(n.size() - 5, 5, ".gguf") != 0 || stat((d + "/" + n).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
                if (std::none_of(out.begin(), out.end(), [&](const TextModel & m) { return m.name == n; })) out.push_back({n, d + "/" + n});
            }
            closedir(h);
        }
    }
    std::sort(out.begin(), out.end(), [](const TextModel & a, const TextModel & b) { return a.name < b.name; });
    return out;
}

std::string resolve_text_model(const std::vector<std::string> & dirs, const std::string & name) {
    if (name.empty()) return find_text_model(dirs);
    for (const auto & m : list_text_models(dirs)) if (m.name == name) return m.path;
    return "";
}

bool needs_other_model(const TextJob & job, const std::string & loaded, const std::vector<std::string> & dirs) {
    if (job.model.empty()) return false;
    const std::string want = resolve_text_model(dirs, job.model);
    return !want.empty() && want != loaded;
}

std::string text_status(const std::string & jobs, const std::string & dir, const std::string & id) {
    json j = {{"id", id}};
    const std::string txt = dir + "/" + id + ".txt", done = dir + "/" + id + ".textdone";
    if (exists(done)) {
        json d = json::parse(slurp(done), nullptr, false);
        j["status"] = d.is_object() && d.contains("status") ? d["status"] : json("failed");
        if (d.is_object() && d.contains("error")) j["error"] = d["error"];
    } else if (exists(jobs + "/" + id + ".textrunning")) {
        j["status"] = exists(txt) ? "generating" : "loading";
    } else if (exists(jobs + "/" + id + ".textjob")) {
        j["status"] = "queued";
    } else {
        return "";
    }
    j["text"] = exists(txt) ? slurp(txt) : "";
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

}  // namespace mg
