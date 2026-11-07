// The job queue's rules, apart from the console so they are tested on a PC (tools/test-queue): the kinds of job files
// in /data/PAUL3D/jobs, which one runs next, and when the worker leaves the text mode (and the title restarts).
#pragma once
#include <string>
#include <vector>

namespace mg {

enum class JobKind { mesh, text, download, unknown };

// "<id>.job" mesh, "<id>.textjob" text, "<id>.dljob" download; anything else unknown.
JobKind kind_of(const std::string & file_name);

// The job file with the smallest name among the three kinds; "" when there is none.
std::string next_job(const std::vector<std::string> & names);

enum class Leave { stay, restart };
constexpr double kTextIdleSeconds = 300;  // the text mode ends after this long without a request

// In the text mode: what to do given the next job's kind (unknown: none) and the seconds without a request.
Leave text_mode_decision(JobKind next, double idle_s);

}  // namespace mg
