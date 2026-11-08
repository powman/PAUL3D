// The job queue's rules, apart from the console so they are tested on a PC (tools/test-queue): the kinds of job files
// in /data/PAUL3D/jobs, which one runs next, and what the worker does with it while the text model is loaded.
#pragma once
#include <string>
#include <vector>

namespace mg {

enum class JobKind { text, download, unknown };

// "<id>.textjob" text, "<id>.dljob" download; anything else unknown.
JobKind kind_of(const std::string & file_name);

// The job file with the smallest name among the kinds above; "" when there is none.
std::string next_job(const std::vector<std::string> & names);

enum class Next { answer, download, wait, restart };
// The text model stays loaded for as long as the title runs: a request for another model is the only reason to start
// over (two models do not fit, and freed GPU memory does not come back to the pool on the console).
Next text_mode_next(JobKind next, bool other_model);

}  // namespace mg
