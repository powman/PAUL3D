// PAUL3D: a part from a sentence. A language model (llama.cpp on ggml-vulkan, mg_text) writes OpenSCAD for the web page,
// which compiles it. Startup patterns are PS5SD's (ps5/app/main.cpp, adapted from PS5LM, GPL-3.0): Vulkan forwarders,
// log to a file, quit file.
//
// /data/PAUL3D/jobs/<id>.textjob  a text request (mg_textjob.hpp); <id>.dljob a model download (mg_download.hpp)
// /data/PAUL3D/app.log    everything the run prints
// /data/PAUL3D/quit       create it and the title exits itself
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "mg_download.hpp"
#include "mg_fetch.hpp"
#include "mg_models_txt.h"  // ps5/models.txt (scripts/build-mg.sh)
#include "mg_queue.hpp"
#include "mg_state.hpp"
#include "mg_text.hpp"
#include "mg_textjob.hpp"
#include "nlohmann/json.hpp"
#include "paul3d_threads.h"

// No Vulkan loader on the console: the four entry points ggml-vulkan links against go to RADV.
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char * name);
extern "C" PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char * name) {
    return vk_icdGetInstanceProcAddr(instance, name);
}
extern "C" PFN_vkVoidFunction vk_common_GetDeviceProcAddr(VkDevice device, const char * name);
extern "C" void vk_common_GetPhysicalDeviceFeatures2(VkPhysicalDevice dev, VkPhysicalDeviceFeatures2 * features);
extern "C" void vk_common_CmdCopyBuffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst, uint32_t n,
                                        const VkBufferCopy * regions);
extern "C" PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char * name) {
    return vk_common_GetDeviceProcAddr(device, name);
}
extern "C" void vkGetPhysicalDeviceFeatures2(VkPhysicalDevice dev, VkPhysicalDeviceFeatures2 * features) {
    vk_common_GetPhysicalDeviceFeatures2(dev, features);
}
extern "C" void vkCmdCopyBuffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst, uint32_t n,
                                const VkBufferCopy * regions) {
    vk_common_CmdCopyBuffer(cmd, src, dst, n, regions);
}

extern "C" {
int       sceSystemServiceHideSplashScreen(void);
int       sceSystemServiceLoadExec(const char * path, const char * const * args);
int64_t   sceKernelGetDirectMemorySize(void);
int       sceKernelAvailableDirectMemorySize(int64_t start, int64_t end, size_t align, int64_t * at, size_t * size);
}

#ifdef MG_SCREEN
bool mg_screen_open();
void mg_screen_frame();
#endif

MgState g_mg;
static std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();
double mg_now() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count(); }

namespace {
constexpr const char * kDir  = "/data/PAUL3D";
constexpr const char * kLog  = "/data/PAUL3D/app.log";
constexpr const char * kRaw  = "/data/PAUL3D/app.raw";
constexpr const char * kJobs = "/data/PAUL3D/jobs";
constexpr const char * kLogs = "/data/PAUL3D/logs";
constexpr const char * kQuit = "/data/PAUL3D/quit";
constexpr double       kGiB  = 1024.0 * 1024.0 * 1024.0;

std::atomic<bool> g_done{false};
std::atomic<bool> g_restart{false};  // a job ran in this process: the next one gets a fresh one
std::chrono::steady_clock::time_point g_t0 = std::chrono::steady_clock::now();

// stdout/stderr go into a pipe (or, if there is none, to app.raw through freopen); stamp_log reads them and puts each
// line, with the seconds since start in front, in a queue; write_log writes the queue to app.log and the job's log twice
// a second. After many jobs in a row a single write to a file on the console took up to a second, and when the
// threads that print waited for the disk (the CPU stages printed hundreds of lines), a job took about twice as long.
int g_log_in = -1;  // the pipe's read end, or -1 for app.raw
std::mutex g_wmu;
std::vector<std::pair<std::string, std::string>> g_wq;  // (job, line) not yet on disk

void write_log() {
    FILE *      out    = fopen(kLog, "a");
    FILE *      jobout = nullptr;
    std::string jobname;
    std::vector<std::pair<std::string, std::string>> batch;
    for (;;) {
        usleep(500000);
        {
            std::lock_guard<std::mutex> lk(g_wmu);
            batch.swap(g_wq);
        }
        for (const auto & [job, line] : batch) {
            if (out) std::fputs(line.c_str(), out);
            // The running job's own log: /data/PAUL3D/logs/<job>.log
            if (job != jobname) {
                if (jobout) fclose(jobout);
                jobname = job;
                jobout  = job.empty() ? nullptr : fopen((std::string(kLogs) + "/" + job + ".log").c_str(), "w");
            }
            if (jobout) std::fputs(line.c_str(), jobout);
        }
        batch.clear();
        if (out) fflush(out);
        if (jobout) fflush(jobout);
    }
}

void * stamp_log(void *) {
    std::thread(write_log).detach();
    int         in = g_log_in;
    while (in < 0) {
        in = open(kRaw, O_RDONLY);
        if (in < 0) usleep(100000);
    }
    std::string line;
    char        buf[4096];
    char        stamp[16];
    for (;;) {
        ssize_t n = 0;
        if (g_log_in >= 0) {
            // Never fflush(stdout) here: it would write into the pipe this thread reads, and a full pipe would hang it.
            // The main loop flushes stdout.
            pollfd p{in, POLLIN, 0};
            if (poll(&p, 1, 200) > 0) n = read(in, buf, sizeof buf);
        } else {
            n = read(in, buf, sizeof buf);
            if (n <= 0) {  // app.raw at its end: wait, and push out stdout, which is fully buffered
                usleep(100000);
                fflush(stdout);
            }
        }
        for (ssize_t k = 0; k < n; k++) {
            const char c = buf[k];
            // Progress bars end their lines with \r: a line ends at \n or \r.
            if (c != '\n' && c != '\r') {
                line += c;
                continue;
            }
            // The OpenGL driver of the screen reports its frames ("[ps5-gallium] ...", "[ps5-driver-cycles] ...") every
            // second or so; that is not what the log is for.
            if (line.empty() || line.compare(0, 5, "[ps5-") == 0) {
                line.clear();
                continue;
            }
            {
                std::string cur;
                {
                    std::lock_guard<std::mutex> lk(g_mg.mu);
                    cur = g_mg.job;
                }
                std::snprintf(stamp, sizeof stamp, "[%7.1f] ", mg_now());
                std::lock_guard<std::mutex> lk(g_wmu);
                g_wq.emplace_back(std::move(cur), stamp + line + "\n");
            }
            {
                // What the web interface shows: the stage line ("[3/7] ..."), the progress bar, the last lines.
                std::lock_guard<std::mutex> lk(g_mg.mu);
                if (line.size() > 4 && line[0] == '[' && isdigit(static_cast<unsigned char>(line[1])) && line[2] == '/') {
                    g_mg.stage = line;
                    g_mg.progress.clear();
                } else if (line.find("[flow] [") != std::string::npos) {
                    g_mg.progress = line;
                } else if (line.find("dit-fwd") == std::string::npos && line.find("trellis:dbg") == std::string::npos &&
                           line.compare(0, 5, "radv/") != 0 && line.compare(0, 4, "[Sce") != 0) {
                    g_mg.tail.push_back(line.substr(0, 200));
                    if (g_mg.tail.size() > 200) g_mg.tail.erase(g_mg.tail.begin(), g_mg.tail.begin() + 100);
                }
            }
            line.clear();
        }
    }
    return nullptr;
}

void log_memory(const char * when) {
    int64_t       at   = 0;
    size_t        free = 0;
    const int64_t pool = sceKernelGetDirectMemorySize();
    sceKernelAvailableDirectMemorySize(0, pool, 2 * 1024 * 1024, &at, &free);
    std::printf("paul3d: memory %s: pool free %.2f of %.2f GiB", when, free / kGiB, pool / kGiB);
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        size_t dev_free = 0, dev_total = 0;
        ggml_backend_dev_memory(ggml_backend_dev_get(i), &dev_free, &dev_total);
        std::printf(", %s free %zu total %zu", ggml_backend_dev_name(ggml_backend_dev_get(i)), dev_free, dev_total);
    }
    std::printf("\n");
    std::fflush(stdout);
}

// "bench-read <file> <block bytes>": how fast the console reads a file (first read of it, so not from cache), with
// read() and blocks of that size, for at most 8 seconds.
std::vector<std::string> job_files() {
    std::vector<std::string> names;
    if (DIR * d = opendir(kJobs)) {
        while (dirent * e = readdir(d)) names.push_back(e->d_name);
        closedir(d);
    }
    return names;
}

// The next job of any kind (mesh, text, download): mg_queue.hpp.
std::string next_job() { return mg::next_job(job_files()); }

std::string slurp_file(const std::string & p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

// One text request with the model loaded: the answer goes to text/<id>.txt as it is made (mg_textjob.hpp).
void run_text_job(mg::TextEngine & eng, const std::string & name) {
    const std::string id = name.substr(0, name.size() - 8), base = std::string(kJobs) + "/" + id;
    rename((base + ".textjob").c_str(), (base + ".textrunning").c_str());
    mg::TextJob job;
    std::string err;
    if (!mg::parse_text_job(slurp_file(base + ".textrunning"), job, err)) {
        mg::write_text_done(kMgText, id, "failed", err);
        unlink((base + ".textrunning").c_str());
        return;
    }
    {
        std::lock_guard<std::mutex> lk(g_mg.mu);
        g_mg.job = id; g_mg.stage = "respondendo: " + eng.model_name(); g_mg.job_t0 = mg_now();
    }
    std::printf("paul3d: TEXT %s START (%zu messages, max %d tokens)\n", id.c_str(), job.messages.size(), job.max_tokens);
    const double t0 = mg_now();
    double last = t0;
    size_t pieces = 0;
    std::string so_far, out;
    const bool ok = eng.chat(job.messages, job.max_tokens, job.temperature, [&](const std::string & p) {
        so_far += p; pieces++;
        if (mg_now() - last > 0.25) { mg::write_text_partial(kMgText, id, so_far); last = mg_now(); }
        return true;
    }, out, err);
    mg::write_text_partial(kMgText, id, out);
    mg::write_text_done(kMgText, id, ok ? "done" : "failed", ok ? "" : err);
    unlink((base + ".textrunning").c_str());
    const double secs = mg_now() - t0;
    std::printf("paul3d: TEXT %s DONE ok=%d %zu tokens in %.1f s (%.1f tokens/s)\n", id.c_str(), ok, pieces, secs, secs > 0 ? pieces / secs : 0.0);
    std::lock_guard<std::mutex> lk(g_mg.mu);
    g_mg.job.clear(); g_mg.progress.clear();
}

// ---- downloads: a .dljob names a group of ps5/models.txt; its files come from Hugging Face (mg_download, mg_fetch).
// The state goes to downloads/<id>.json for the web page; downloads/<id>.cancel stops it. No restart afterwards.
void save_download(nlohmann::json & st) {
    mkdir(kMgDownloads, 0777);
    const std::string p = std::string(kMgDownloads) + "/" + st.value("id", "x") + ".json";
    { std::ofstream f(p + ".tmp"); f << st.dump(); }
    rename((p + ".tmp").c_str(), p.c_str());
}

void run_download(const std::string & name) {
    const std::string id = name.substr(0, name.size() - 6), base = std::string(kJobs) + "/" + id;
    rename((base + ".dljob").c_str(), (base + ".dlrunning").c_str());
    const std::string cancel = std::string(kMgDownloads) + "/" + id + ".cancel";
    auto req = nlohmann::json::parse(slurp_file(base + ".dlrunning"), nullptr, false);
    auto st  = nlohmann::json::parse(slurp_file(std::string(kMgDownloads) + "/" + id + ".json"), nullptr, false);
    if (!st.is_object()) st = {{"id", id}};
    const std::string group = req.is_object() ? req.value("group", "") : "";
    std::string err;
    const auto cat = mg::parse_catalog(std::string(reinterpret_cast<const char *>(mg_models_txt), mg_models_txt_len), err);
    std::vector<mg::ModelFile> files;
    int64_t total = 0, base_done = 0;
    for (const auto & f : cat) if (f.group == group) { files.push_back(f); total += f.size; }
    st["group"] = group; st["total"] = total; st["state"] = "downloading"; st["error"] = nullptr;
    std::printf("paul3d: DOWNLOAD %s START %s (%zu files, %.2f GB)\n", id.c_str(), group.c_str(), files.size(), total / 1e9);
    bool ok = !files.empty();
    if (!ok) err = "grupo desconhecido: " + group;
    const double t0 = mg_now();
    for (const auto & f : files) {
        if (!ok) break;
        st["file"] = f.path;
        double last = 0, last_t = mg_now();
        int64_t last_done = 0;
        auto progress = [&](int64_t done, int64_t size) {
            if (mg_now() - last > 0.5) {
                struct stat cs;
                if (stat(cancel.c_str(), &cs) == 0) return false;
                const double now = mg_now();
                st["done"]  = base_done + done;
                st["speed"] = now > last_t ? (double) (done - last_done) / (now - last_t) : 0.0;
                st["state"] = done >= size ? "verifying" : "downloading";
                last_done = done; last_t = now; last = now;
                save_download(st);
                std::lock_guard<std::mutex> lk(g_mg.mu);
                g_mg.stage = "download " + group + " " + std::to_string((int) (100.0 * (base_done + done) / std::max<int64_t>(total, 1))) + "%";
            }
            return true;
        };
        {
            std::lock_guard<std::mutex> lk(g_mg.mu);
            g_mg.job = id; g_mg.job_t0 = t0;
        }
        ok = mg::download_file(f, kDir, mg::hf_url(f), mg::console_fetch(), mg::free_bytes(kDir), progress, err);
        if (ok) base_done += f.size;
    }
    st["done"] = base_done;
    st["speed"] = 0;
    st["state"] = ok ? "done" : (err == "cancelado" ? "cancelled" : "failed");
    if (!ok) st["error"] = err;
    st.erase("file");
    save_download(st);
    unlink((base + ".dlrunning").c_str());
    unlink(cancel.c_str());
    std::printf("paul3d: DOWNLOAD %s %s in %.0f s%s%s\n", id.c_str(), st.value("state", "").c_str(), mg_now() - t0, ok ? "" : ": ", ok ? "" : err.c_str());
    std::lock_guard<std::mutex> lk(g_mg.mu);
    g_mg.job.clear(); g_mg.progress.clear();
}

// The worker of the text-only title: the text model is loaded when the title starts (the one last asked for) and stays
// loaded; requests are answered one after the other, downloads run in between. A request for another model makes the
// title start over with that one (two models do not fit, and freed GPU memory does not come back to the pool).
void * run(void *) {
    std::printf("paul3d: paul3d_threads=%u hardware_concurrency=%u\n", paul3d_threads(), std::thread::hardware_concurrency());
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        std::printf("paul3d: device %zu: %s\n", i, ggml_backend_dev_name(ggml_backend_dev_get(i)));
    }
    mkdir(kJobs, 0777);
    mkdir(kMgText, 0777);
    mkdir((std::string(kDir) + "/models").c_str(), 0777);
    mkdir((std::string(kDir) + "/models/text").c_str(), 0777);
    // A request or a download still marked running was in the title when it went away (a crash, the console).
    for (const auto & n : job_files()) {
        if (n.size() > 10 && n.compare(n.size() - 10, 10, ".dlrunning") == 0) {
            const std::string id = n.substr(0, n.size() - 10);
            auto st = nlohmann::json::parse(slurp_file(std::string(kMgDownloads) + "/" + id + ".json"), nullptr, false);
            if (!st.is_object()) st = {{"id", id}};
            st["state"] = "failed"; st["error"] = "interrompido quando o app fechou; peça de novo para continuar de onde parou";
            save_download(st);
            unlink((std::string(kJobs) + "/" + n).c_str());
        }
        if (n.size() > 12 && n.compare(n.size() - 12, 12, ".textrunning") == 0) {
            mg::write_text_done(kMgText, n.substr(0, n.size() - 12), "failed", "o app fechou enquanto respondia");
            unlink((std::string(kJobs) + "/" + n).c_str());
        }
    }
    const std::vector<std::string> dirs{std::string(kDir) + "/models/text", "/data/PS5LM/models"};
    const std::string pref = std::string(kDir) + "/text-model.txt";
    auto asked = [&](const std::string & name) {
        mg::TextJob j; std::string e;
        mg::parse_text_job(slurp_file(std::string(kJobs) + "/" + name), j, e);
        return j;
    };
    auto fail_job = [&](const std::string & name, const std::string & error) {
        mg::write_text_done(kMgText, name.substr(0, name.size() - 8), "failed", error);
        unlink((std::string(kJobs) + "/" + name).c_str());
    };
    auto set_stage = [](const std::string & st) { std::lock_guard<std::mutex> lk(g_mg.mu); g_mg.stage = st; };
    std::unique_ptr<mg::TextEngine> eng;
    std::string loaded, load_err;
    auto load = [&](const std::string & path) {
        const std::string name = path.substr(path.find_last_of('/') + 1);
        set_stage("carregando " + name);
        { std::lock_guard<std::mutex> lk(g_mg.mu); g_mg.job_t0 = mg_now(); }
        const double t0 = mg_now();
        eng.reset(new mg::TextEngine);
        // 4096 tokens of context and a batch of 512: a part's request, its answer and up to three fixes fit, and the
        // compute buffers stay small, which a 27B model (9.8 GB of a pool of 11.4 GiB) needs.
        if (!eng->load(path, 4096, (int) paul3d_threads(), load_err)) {
            std::printf("paul3d: text model %s: %s\n", path.c_str(), load_err.c_str());
            eng.reset();
            set_stage("falha ao carregar " + name);
            return false;
        }
        loaded = path;
        mg::remember_text_model(pref, name);
        std::printf("paul3d: text model %s loaded in %.1f s\n", path.c_str(), mg_now() - t0);
        log_memory("text model loaded");
        set_stage("pronto: " + name);
        return true;
    };
    bool load_failed = false;
    if (const std::string p = mg::preferred_text_model(dirs, pref); !p.empty()) load_failed = !load(p);
    else set_stage("sem modelo de texto: baixe em Modelos");
    struct stat st;
    while (stat(kQuit, &st) != 0) {
        const std::string name = next_job();
        const mg::JobKind kind = mg::kind_of(name);
        if (kind == mg::JobKind::text) {
            const mg::TextJob j = asked(name);
            if (!eng) {
                const std::string path = mg::resolve_text_model(dirs, j.model);
                if (path.empty()) { fail_job(name, j.model.empty() ? "missing_model" : "no such model: " + j.model); continue; }
                // One try a start: after a failed load the pool is not clean, and starting over to try again could
                // repeat for ever. The request says why; the next start tries again.
                if (load_failed || !load(path)) { fail_job(name, "não foi possível carregar o modelo: " + load_err); load_failed = true; continue; }
            }
            if (mg::text_mode_next(kind, mg::needs_other_model(j, loaded, dirs)) == mg::Next::restart) {
                std::printf("paul3d: %s asks for %s: starting over with it\n", name.c_str(), j.model.c_str());
                mg::remember_text_model(pref, j.model);
                g_restart = true;
                break;
            }
            run_text_job(*eng, name);
            set_stage("pronto: " + eng->model_name());
            continue;
        }
        if (kind == mg::JobKind::download) {
            run_download(name);
            set_stage(eng ? "pronto: " + eng->model_name() : "sem modelo carregado");
            continue;
        }
        usleep(300000);
    }
    std::printf("paul3d: worker ends\n");
    std::fflush(nullptr);
    g_done = true;
    return nullptr;
}
}  // namespace

int main() {
    sceSystemServiceHideSplashScreen();
    mkdir(kDir, 0777);
    mkdir(kLogs, 0777);
    // A quit file left by an earlier run (a crash, or one written while no title ran) would end this one at once.
    unlink(kQuit);
    // Both in append mode: with "w" for one of them the two write at different offsets and garble each other's lines.
    { FILE * f = fopen(kRaw, "w"); if (f) fclose(f); }
    int fds[2];
    if (pipe(fds) == 0 && dup2(fds[1], 1) >= 0 && dup2(fds[1], 2) >= 0) {
        close(fds[1]);
        g_log_in = fds[0];
    } else {
        freopen(kRaw, "a", stdout);
        freopen(kRaw, "a", stderr);
    }
    pthread_t lt;
    pthread_create(&lt, nullptr, stamp_log, nullptr);
    // Whole lines to the file: unbuffered, the console's libc writes a character at a time and the two streams
    // garble each other. stdout is flushed by the log thread, so the progress bars (which end in a carriage return) still show.
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    setvbuf(stderr, nullptr, _IOLBF, 1 << 12);
    setenv("radv_enable_unified_heap_on_apu", "true", 1);
    std::printf("paul3d: start\n");
#ifdef MG_SCREEN
    // The screen first: it takes from the same memory pool as the models.
    const bool screen = mg_screen_open();
#else
    const bool screen = false;
#endif

    // 16 MiB stack: the pipeline recurses deeper than the main thread's default allows.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16u << 20);
    pthread_t th;
    if (pthread_create(&th, &attr, run, nullptr) != 0) {
        std::printf("paul3d: cannot start the worker\n");
        g_done = true;
    }

    // The web interface, in a thread of its own; it ends with the title.
    pthread_attr_t wattr;
    pthread_attr_init(&wattr);
    pthread_attr_setstacksize(&wattr, 4u << 20);
    pthread_t web;
    pthread_create(&web, &wattr, [](void *) -> void * { mg_web_run(7870); return nullptr; }, nullptr);

    // The screen (or, without one, a pause) until the title ends itself: when asked (the quit file), or to load
    // another text model.
    double next_pool = 0;
    double next_out = 0;
    while (!g_done) {
        if (g_log_in >= 0 && mg_now() >= next_out) {  // stdout is fully buffered; the log thread may not flush it (see stamp_log)
            next_out = mg_now() + 0.1;
            fflush(stdout);
        }
        if (mg_now() >= next_pool) {
            next_pool = mg_now() + 1;
            int64_t       at   = 0;
            size_t        free = 0;
            const int64_t pool = sceKernelGetDirectMemorySize();
            sceKernelAvailableDirectMemorySize(0, pool, 2 * 1024 * 1024, &at, &free);
            std::lock_guard<std::mutex> lk(g_mg.mu);
            g_mg.pool_free_gib = free / kGiB;
            g_mg.pool_gib      = pool / kGiB;
            g_mg.threads       = paul3d_threads();
        }
#ifdef MG_SCREEN
        if (screen) {
            mg_screen_frame();
            continue;
        }
#endif
        usleep(200000);
    }
    if (g_restart) {
        std::printf("paul3d: restarting to load another text model\n");
        std::fflush(nullptr);
        usleep(1500000);  // the log thread copies the last lines
        const char * none[] = {nullptr};
        const int    r      = sceSystemServiceLoadExec("/app0/eboot.bin", none);
        std::printf("paul3d: LoadExec of /app0/eboot.bin returned 0x%08x\n", static_cast<unsigned>(r));
        std::fflush(nullptr);
        usleep(1500000);
    }
    unlink(kQuit);
    std::fflush(nullptr);
    sceSystemServiceLoadExec("exit", nullptr);
    return 0;
}
