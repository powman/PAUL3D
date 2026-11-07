// Models the title downloads itself (spec: "Download de modelos pelo app"): the catalogue (ps5/models.txt), what is
// on disk, and one file's download with resume (<dest>.part) and a sha256 check before it is used.
// No sockets in here: the caller gives a Fetch (PS5SD's HttpClient on the console, cpp-httplib in tools/test-download).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mg {

struct ModelFile {
    std::string group, sha256, repo, rev, path, dest, licence;
    int64_t     size = 0;
};

// "group sha256 size repo revision path dest licence" a line; # lines and empty lines are skipped.
std::vector<ModelFile> parse_catalog(const std::string & text, std::string & err);
std::string hf_url(const ModelFile & f);  // https://huggingface.co/<repo>/resolve/<rev>/<path>

struct GroupState {
    std::string group;
    int64_t     total = 0, have = 0;  // have: bytes on disk, finished files and .part files
    bool        installed = false;    // every file there in full
};
std::vector<GroupState> catalog_state(const std::vector<ModelFile> & files, const std::string & root);

// GET url from byte `from` on (a Range when from > 0); the body goes to sink (false from sink stops it). total: the
// whole file's size as the response says it. False with err on a network failure or an HTTP error.
using Fetch = std::function<bool(const std::string & url, int64_t from, const std::function<bool(const char *, size_t)> & sink,
                                 int64_t & total, std::string & err)>;

// Downloads f to <root>/<f.dest>: appends to <dest>.part (resume), checks the size and sha256, renames. Refuses before
// connecting when free_bytes is less than what is missing. progress(done, total) returning false cancels it.
// On a wrong sha256 the file is deleted. False with err (for the person) otherwise.
bool download_file(const ModelFile & f, const std::string & root, const std::string & url, const Fetch & fetch,
                   int64_t free_bytes, const std::function<bool(int64_t, int64_t)> & progress, std::string & err);

int64_t free_bytes(const std::string & path);  // bytes free on the disk of path; -1 when unknown

std::string sha256_hex(const std::string & data);  // for tests and small data
std::string sha256_file(const std::string & path);  // "" when the file cannot be read

}  // namespace mg
