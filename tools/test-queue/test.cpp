// Tests of ps5/mg_queue (job kinds, which job is next, when the text mode ends). Built and run by run.sh on the PC.
#include "mg_queue.hpp"
#include <cstdio>
#include <string>

static int fails = 0;
template <class A, class B> static void t(const char * name, const A & got, const B & want) {
    const bool ok = got == want;
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) fails++;
}

int main() {
    using mg::JobKind; using mg::Leave;
    t("mesh job", mg::kind_of("w1.job"), JobKind::mesh);
    t("text job", mg::kind_of("t1.textjob"), JobKind::text);
    t("download job", mg::kind_of("d1.dljob"), JobKind::download);
    t("tmp file is not a job", mg::kind_of("w1.job.tmp"), JobKind::unknown);
    t("next is the smallest name", mg::next_job({"w2.job", "w1.textjob", "w3.dljob", "x.meta"}), std::string("w1.textjob"));
    t("no job", mg::next_job({"a.meta", "b.done"}), std::string());
    t("text then text: stay", mg::text_mode_decision(JobKind::text, 0), Leave::stay);
    t("text then mesh: restart", mg::text_mode_decision(JobKind::mesh, 0), Leave::restart);
    t("text then download: restart", mg::text_mode_decision(JobKind::download, 0), Leave::restart);
    t("idle 299 s: stay", mg::text_mode_decision(JobKind::unknown, 299), Leave::stay);
    t("idle 300 s: restart", mg::text_mode_decision(JobKind::unknown, 300), Leave::restart);
    return fails ? 1 : 0;
}
