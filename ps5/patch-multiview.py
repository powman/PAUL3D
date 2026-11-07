#!/usr/bin/env python3
"""More pictures of the same object for one TRELLIS.2 run: trellis-cli --view back.png [--multiview stochastic|mean].

Training-free, as TRELLIS v1's run_multi_image: every extra picture is cut out and encoded by DINOv3 like the main
one, and sample_flow mixes the conditions in. "stochastic" uses view (step % n) at each step (same time); "mean"
averages the velocity over all views (n times the flow forwards). TRELLIS.2 itself has no multi-image mode, so this
is an experiment: compare a run with and without the back picture, same seed.

usage: patch-multiview.py <trellis.cpp dir>   (scripts/build-mg.sh; does nothing when already applied)
"""
import os
import sys

T = sys.argv[1]
MARK = "g_multiview"


def patch(rel, pairs):
    p = os.path.join(T, rel)
    s = open(p, encoding="utf-8").read()
    if MARK in s or "multiview_mode" in s:
        return
    for old, new in pairs:
        if s.count(old) != 1:
            sys.exit(f"patch-multiview: {rel}: anchor not found once: {old[:70]!r}")
        s = s.replace(old, new)
    open(p, "w", encoding="utf-8").write(s)
    print(f"patch-multiview: {rel}")


patch("include/flow_runner.h", [(
    "// FlowEuler guidance-interval sampler over an arbitrary forward functor.",
    """// Multi-view conditioning (training-free, as TRELLIS v1's run_multi_image): more pictures of the
// same object, e.g. its back. Keyed by the primary positive cond pointer; sample_flow swaps in the
// other views' conds of the same shape. mode 0 "stochastic": view (step % n) at each step, same cost.
// mode 1 "mean" (multidiffusion): the mean velocity over all views, n times the forwards.
struct MultiView {
    std::map<const float*, std::vector<const float*>> alt;
    int mode = 0;
};
extern MultiView g_multiview;

// FlowEuler guidance-interval sampler over an arbitrary forward functor.""")])

patch("src/flow_runner.cpp", [
    ("std::vector<float> sample_flow(const FlowFwd& fwd, std::vector<float> sample,",
     "MultiView g_multiview;\n\nstd::vector<float> sample_flow(const FlowFwd& fwd, std::vector<float> sample,"),
    ("""    progress(0);
    for (int i = 0; i < sp.steps; ++i) {""", """    std::vector<const float*> views{cond.cond};
    if (auto it = g_multiview.alt.find(cond.cond); it != g_multiview.alt.end())
        views.insert(views.end(), it->second.begin(), it->second.end());
    if (views.size() > 1)
        printf("      [flow] %zu views, %s\\n", views.size(), g_multiview.mode ? "mean" : "stochastic");
    int step_i = 0;
    auto fwd_pos = [&](const std::vector<float>& x, float ts_) -> std::vector<float> {
        if (views.size() == 1) return fwd(x, ts_, cond);
        FlowCond c = cond;
        if (g_multiview.mode == 0) { c.cond = views[step_i % views.size()]; return fwd(x, ts_, c); }
        std::vector<float> acc;
        for (const float* v : views) {
            c.cond = v; std::vector<float> o = fwd(x, ts_, c);
            if (acc.empty()) acc = std::move(o); else for (size_t k = 0; k < acc.size(); ++k) acc[k] += o[k];
        }
        for (float& a : acc) a /= (float)views.size();
        return acc;
    };
    progress(0);
    for (int i = 0; i < sp.steps; ++i) {
        step_i = i;"""),
    ("pred = fwd(sample, tscaled, cond);", "pred = fwd_pos(sample, tscaled);"),
    ("pos = fwd(sample, tscaled, cond);", "pos = fwd_pos(sample, tscaled);"),
])

patch("include/trellis_args.h", [(
    "    std::string image;                                          // input image (image->3D)",
    """    std::string image;                                          // input image (image->3D)
    std::vector<std::string> views;  // --view: more pictures of the same object (its back, ...), trellis family
    int multiview_mode = 0;          // --multiview stochastic (0) | mean (1)""")])
h = os.path.join(T, "include/trellis_args.h")
s = open(h, encoding="utf-8").read()
if "#include <vector>" not in s:
    open(h, "w", encoding="utf-8").write(s.replace("#include <string>", "#include <string>\n#include <vector>", 1))

patch("src/trellis_args.cpp", [
    ('        else if (a == "-o" || a == "--output")',
     """        else if (a == "--view")                 { const char* v = need(a.c_str()); if (!v) return false; p.views.push_back(v); }
        else if (a == "--multiview")            { const char* v = need(a.c_str()); if (!v) return false;
                                                  if (!std::strcmp(v, "stochastic")) p.multiview_mode = 0;
                                                  else if (!std::strcmp(v, "mean")) p.multiview_mode = 1;
                                                  else { fprintf(stderr, "--multiview: stochastic or mean\\n"); return false; } }
        else if (a == "-o" || a == "--output")"""),
    ('        "  -i, --image PATH        input image                  (image->3D)\\n"',
     """        "  -i, --image PATH        input image                  (image->3D)\\n"
        "      --view PATH         another picture of the same object (e.g. its back); repeatable\\n"
        "      --multiview MODE    with --view: stochastic (one view per step, default) or mean\\n\""""),
])

patch("src/trellis_cli.cpp", [(
    '    if (cascade) slat_stats("cond_1024 (DINOv3@1024)", cond1024);\n',
    '''    if (cascade) slat_stats("cond_1024 (DINOv3@1024)", cond1024);

    // --view: more pictures of the same object (its back, a side). Cut out and encoded exactly like
    // the main one, then registered so sample_flow mixes them in (flow_runner.h, MultiView).
    // The registry is global (trellis-server runs many jobs in one process): cleared on every exit.
    struct MultiViewReset { ~MultiViewReset() { trellis::g_multiview = trellis::MultiView{}; } } multiview_reset;
    vector<vector<float>> vcond, vcond1024;   // must outlive every sample_flow below
    if (!cfg.views.empty()) {
        if (pix) { fprintf(stderr, "[trellis] --view works with --model trellis only\\n"); return 1; }
        FILE* bf = fopen((M + "/birefnet.gguf").c_str(), "rb"); const bool have_bf = bf != nullptr; if (bf) fclose(bf);
        vector<vector<unsigned char>> vcut(cfg.views.size()); vector<int> vsz(cfg.views.size(), 0);
        for (size_t v = 0; v < cfg.views.size(); ++v) {
            const std::string& vp = cfg.views[v];
            const bool vb = cfg.birefnet == 1 || (cfg.birefnet < 0 && have_bf && !trellis::image_has_alpha(vp));
            printf("[1/6] view %zu: preprocess %s (%s)\\n", v + 1, vp.c_str(), vb ? "BiRefNet" : "threshold/alpha");
            if (vb) {
                trellis::Model bm = trellis::Model::load(M + "/birefnet.gguf", gpu);
                vcut[v] = trellis::birefnet_cutout(vp, bm, gpu < 0 ? 0 : gpu, vsz[v]);
                bm.free();
            } else {
                vcut[v] = trellis::threshold_cutout(vp, vsz[v]);
            }
            if (vcut[v].empty()) return 1;
            if (cfg.dump_bg) {
                const std::string cp = outglb.substr(0, outglb.find_last_of('.')) + "_view" + std::to_string(v + 1) + "_cutout.png";
                const int ch = vcut[v].size() == (size_t)vsz[v] * vsz[v] * 4 ? 4 : 3;
                if (stbi_write_png(cp.c_str(), vsz[v], vsz[v], ch, vcut[v].data(), vsz[v] * ch)) printf("      cutout -> %s\\n", cp.c_str());
            }
        }
        printf("[2/6] DINOv3 conditioning, %zu more view(s)\\n", cfg.views.size());
        trellis::Model m = trellis::Model::load(M + "/dinov3.gguf", gpu);
        for (size_t v = 0; v < cfg.views.size(); ++v) {
            vcond.push_back(trellis::dinov3_encode(m, trellis::normalize_cutout(vcut[v], vsz[v], 512), 512));
            if (cascade) vcond1024.push_back(trellis::dinov3_encode(m, trellis::normalize_cutout(vcut[v], vsz[v], 1024), 1024));
        }
        m.free();
        for (size_t v = 0; v < vcond.size(); ++v) {
            if (vcond[v].size() != cond.size() || (cascade && vcond1024[v].size() != cond1024.size())) {
                fprintf(stderr, "[trellis] view %zu: token count differs from the main picture\\n", v + 1); return 1; }
            trellis::g_multiview.alt[cond.data()].push_back(vcond[v].data());
            if (cascade) trellis::g_multiview.alt[cond1024.data()].push_back(vcond1024[v].data());
        }
        trellis::g_multiview.mode = cfg.multiview_mode;
    }
''')])
