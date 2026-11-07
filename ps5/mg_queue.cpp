#include "mg_queue.hpp"

#include <algorithm>

namespace mg {

static bool ends_with(const std::string & s, const std::string & tail) {
    return s.size() > tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

JobKind kind_of(const std::string & n) {
    if (ends_with(n, ".textjob")) return JobKind::text;
    if (ends_with(n, ".dljob")) return JobKind::download;
    if (ends_with(n, ".job")) return JobKind::mesh;
    return JobKind::unknown;
}

std::string next_job(const std::vector<std::string> & names) {
    std::string best;
    for (const auto & n : names) {
        if (kind_of(n) != JobKind::unknown && (best.empty() || n < best)) best = n;
    }
    return best;
}

Leave text_mode_decision(JobKind next, double idle_s) {
    if (next == JobKind::text) return Leave::stay;
    if (next == JobKind::mesh || next == JobKind::download) return Leave::restart;
    return idle_s >= kTextIdleSeconds ? Leave::restart : Leave::stay;
}

}  // namespace mg
