// PAUL3D's screen: PS5SD's dashboard (ps5/screen, adapted from PS5LM and PS5SD, GPL-3.0) fed from the worker's state
// (mg_main.cpp): the text model, what the console is doing (loading, answering, downloading), the log, the address.
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

#include <GL/glcorearb.h>

#include "app_status.hpp"
#include "core/input.hpp"
#include "dashboard.hpp"
#include "display.hpp"
#include "gfx/renderer.hpp"
#include "mg_state.hpp"
#include "platform/ps5/pad.hpp"

extern "C" {
int sceKernelUsleep(unsigned int microseconds);
}

namespace {

constexpr const char * kFont    = "/app0/assets/fonts/dejavu-sans-mono.huifont";
constexpr const char * kVersion = "0.2.4";
constexpr int          kWebPort = 7870;

ps5sd::AppStatus g_status;
ps5sd::Display   g_display;
hui::gfx::Renderer g_renderer;
ps5sd::Dashboard g_view;
bool             g_ui_ok = false;
hui::ps5::Pad    g_pad;
hui::InputTracker g_input;

std::string slurp(const std::string & p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The console's address on the local network (PS5SD's lan_address): a UDP socket "connected" to an outside
// address sends nothing, and getsockname() names the interface it would leave by.
std::string lan_address() {
    const int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return {};
    sockaddr_in to     = {};
    to.sin_family      = AF_INET;
    to.sin_port        = htons(53);
    to.sin_addr.s_addr = htonl(0x08080808);
    sockaddr_in local  = {};
    socklen_t   len    = sizeof(local);
    std::string out;
    if (connect(s, reinterpret_cast<const sockaddr *>(&to), sizeof(to)) == 0 &&
        getsockname(s, reinterpret_cast<sockaddr *>(&local), &len) == 0) {
        const uint32_t a = ntohl(local.sin_addr.s_addr);
        if (a != 0 && (a >> 24) != 127) {
            char text[16];
            std::snprintf(text, sizeof(text), "%u.%u.%u.%u", a >> 24, (a >> 16) & 255u, (a >> 8) & 255u, a & 255u);
            out = text;
        }
    }
    close(s);
    return out;
}

}  // namespace

bool mg_screen_open() {
    // The screen first: it takes from the same memory pool as the models.
    if (!g_display.open(1920, 1080)) {
        std::printf("paul3d: no display\n");
        return false;
    }
    g_ui_ok = g_renderer.init() && g_view.load(g_renderer, kFont);
    std::printf("paul3d: pad %s\n", g_pad.open() ? "open" : "not available");
    std::printf("paul3d: screen %s\n", g_ui_ok ? "ready" : "failed");
    g_status.set_model("IA de texto (OpenSCAD)");
    g_status.set_port(kWebPort);
    g_status.set_phase("Opening the GPU");
    return g_ui_ok;
}

// One frame; the caller loops (the main thread, until the title ends).
void mg_screen_frame() {
    using clock = std::chrono::steady_clock;
    static auto        last   = clock::now();
    static size_t      shown_tail = 0;
    static double      next_addr = 0;
    static std::string address;

    // What the worker says (a copy: the mutex is held briefly).
    std::string job, stage, progress;
    std::vector<std::string> tail;
    double free_gib, pool_gib;
    {
        std::lock_guard<std::mutex> lk(g_mg.mu);
        job = g_mg.job; stage = g_mg.stage; progress = g_mg.progress; tail = g_mg.tail;
        free_gib = g_mg.pool_free_gib; pool_gib = g_mg.pool_gib;
    }
    g_status.set_memory(free_gib, pool_gib);

    if (mg_now() >= next_addr) {
        address   = lan_address();
        next_addr = mg_now() + (address.empty() ? 5 : 60);
        g_status.set_address(address);
    }

    // The stage as the phase: "carregando <model>", "pronto: <model>", "respondendo: <model>", a download.
    static std::string last_phase;
    const std::string phase = stage.empty() ? "Ready" : stage.substr(0, 70);
    if (phase != last_phase) {
        last_phase = phase;
        if (job.empty()) g_status.set_phase("Ready", phase);
        else g_status.set_phase("Generating", phase);
    }

    // The log tail: lines the dashboard has not seen.
    if (tail.size() < shown_tail) shown_tail = 0;
    for (size_t i = shown_tail; i < tail.size(); i++) g_status.log(tail[i]);
    shown_tail = tail.size();

    const auto now = clock::now();

    // The controller: read so its queue does not fill up; no actions on this screen.
    hui::PadSample samples[64];
    const size_t   n_samples = g_pad.read(samples);
    g_input.update({samples, n_samples}, static_cast<uint64_t>(mg_now() * 1e6));
    g_view.set_hints({});
    g_view.update(g_status.snapshot(), std::chrono::duration<float>(now - last).count());
    last = now;
    g_view.compose(g_renderer, kVersion);
    g_display.present(g_renderer);
    if (!g_display.swap()) sceKernelUsleep(16 * 1000);
    // While the model answers the screen redraws about 8 times a second: at 60 the GL driver takes GPU time from it
    // (measured with the 3D engine: 10 percent).
    if (!job.empty()) sceKernelUsleep(110 * 1000);
}
