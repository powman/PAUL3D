#include "mg_queue.hpp"

namespace mg {

static bool ends_with(const std::string & s, const std::string & tail) {
    return s.size() > tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

JobKind kind_of(const std::string & n) {
    if (ends_with(n, ".textjob")) return JobKind::text;
    if (ends_with(n, ".dljob")) return JobKind::download;
    return JobKind::unknown;
}

std::string next_job(const std::vector<std::string> & names) {
    std::string best;
    for (const auto & n : names) {
        if (kind_of(n) != JobKind::unknown && (best.empty() || n < best)) best = n;
    }
    return best;
}

Next text_mode_next(JobKind next, bool other_model) {
    if (next == JobKind::text) return other_model ? Next::restart : Next::answer;
    if (next == JobKind::download) return Next::download;
    return Next::wait;
}

}  // namespace mg
