// Tests of ps5/mg_queue (job kinds, which job is next, when the text mode ends). Built and run by run.sh on the PC.
// Text-only title: the model stays loaded; only a request for another model makes the title start over.
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
    using mg::JobKind; using mg::Next;
    t("text job", mg::kind_of("t1.textjob"), JobKind::text);
    t("download job", mg::kind_of("d1.dljob"), JobKind::download);
    t("an old mesh job is not a job any more", mg::kind_of("w1.job"), JobKind::unknown);
    t("tmp file is not a job", mg::kind_of("t1.textjob.tmp"), JobKind::unknown);
    t("next is the smallest name", mg::next_job({"t2.textjob", "d1.dljob", "w0.job", "x.meta"}), std::string("d1.dljob"));
    t("no job", mg::next_job({"a.meta", "b.done"}), std::string());
    t("text, same model: answer it", mg::text_mode_next(JobKind::text, false), Next::answer);
    t("text, another model: start over", mg::text_mode_next(JobKind::text, true), Next::restart);
    t("download: run it, model stays", mg::text_mode_next(JobKind::download, false), Next::download);
    t("nothing: wait, model stays", mg::text_mode_next(JobKind::unknown, false), Next::wait);
    return fails ? 1 : 0;
}
