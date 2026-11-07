// A text request between the web server and the worker, as files (the queue is on disk, like the mesh jobs):
//   jobs/<id>.textjob      the request (POST /api/text's JSON), .textrunning while the worker has it
//   text/<id>.txt          the answer so far, rewritten while it is made
//   text/<id>.textdone     {"status":"done"|"failed","error":...} at the end
// Tested on a PC by tools/test-queue.
#pragma once
#include <string>
#include <vector>

#include "mg_text.hpp"

namespace mg {

struct TextJob {
    std::vector<ChatMsg> messages;
    int   max_tokens  = 4096;  // clamped to 1..8192
    float temperature = 0.2f;  // clamped to 0..2
};

constexpr size_t kTextJobMaxChars = 200000;

// Checks and reads a request: JSON, a non-empty "messages" of {role: system|user|assistant, content}, at most
// kTextJobMaxChars characters of content in all. False with a message for the person otherwise.
bool parse_text_job(const std::string & json, TextJob & out, std::string & err);

void write_text_partial(const std::string & text_dir, const std::string & id, const std::string & text);  // atomic
void write_text_done(const std::string & text_dir, const std::string & id, const std::string & status, const std::string & error);

// The first .gguf (by name) of the first folder that has one; "" when none. The worker loads it; the web server
// answers missing_model without it. Folders: models/text of PAUL3D, then PS5LM's models.
std::string find_text_model(const std::vector<std::string> & dirs);

// {"id","status":"queued"|"loading"|"generating"|"done"|"failed","text","error"?}; "" when the id is unknown.
std::string text_status(const std::string & jobs_dir, const std::string & text_dir, const std::string & id);

}  // namespace mg
