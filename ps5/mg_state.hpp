// What the worker (mg_main.cpp) tells the web server (mg_web.cpp) and what the web server needs of the title.
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

struct MgState {
    std::mutex               mu;
    std::string              job;       // the job running now, "" when idle
    std::string              stage;     // "[3/7] shape SLAT flow (512)"
    std::string              progress;  // the last flow progress bar line
    double                   job_t0 = 0;       // seconds since start, when the job began
    std::vector<std::string> tail;      // last log lines
    double                   pool_free_gib = 0, pool_gib = 0;
    unsigned                 threads = 0;
    std::atomic<bool>        viewing{false};  // a model is on the TV: the worker starts no job meanwhile
};

extern MgState g_mg;
double         mg_now();  // seconds since the title started

constexpr const char * kMgDir  = "/data/PAUL3D";
constexpr const char * kMgJobs = "/data/PAUL3D/jobs";
constexpr const char * kMgIn   = "/data/PAUL3D/in";
constexpr const char * kMgOut  = "/data/PAUL3D/out";
constexpr const char * kMgText = "/data/PAUL3D/text";   // answers of the text engine (mg_textjob.hpp)

// Serves the web interface; returns when the server stops.
void mg_web_run(int port);
