#include "mg_download.hpp"

#include <sys/stat.h>
#include <sys/statvfs.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <vector>

namespace mg {

// ---- sha256 (FIPS 180-4), small and without mbedTLS so this file builds on any PC
namespace {
struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    unsigned char buf[64];
    size_t   n   = 0;
    uint64_t len = 0;
    static uint32_t rotr(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
    void block(const unsigned char * p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t) p[4 * i] << 24 | (uint32_t) p[4 * i + 1] << 16 | (uint32_t) p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; i++) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            const uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void update(const void * data, size_t size) {
        const unsigned char * p = (const unsigned char *) data;
        len += size;
        while (size) {
            const size_t take = std::min(size, 64 - n);
            std::memcpy(buf + n, p, take);
            n += take; p += take; size -= take;
            if (n == 64) { block(buf); n = 0; }
        }
    }
    std::string hex() {
        const uint64_t bits = len * 8;
        const unsigned char pad = 0x80, zero = 0;
        update(&pad, 1);
        while (n != 56) update(&zero, 1);
        unsigned char l[8];
        for (int i = 0; i < 8; i++) l[i] = (unsigned char) (bits >> (56 - 8 * i));
        update(l, 8);
        char out[65];
        for (int i = 0; i < 8; i++) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
        return out;
    }
};

int64_t size_of(const std::string & p) { struct stat st; return stat(p.c_str(), &st) == 0 ? (int64_t) st.st_size : -1; }

void mkdirs(const std::string & dir) {
    for (size_t i = 1; i <= dir.size(); i++) {
        if (i == dir.size() || dir[i] == '/') mkdir(dir.substr(0, i).c_str(), 0777);
    }
}

std::string gb(int64_t bytes) { char s[32]; std::snprintf(s, sizeof s, "%.1f GB", bytes / 1e9); return s; }
}  // namespace

std::string sha256_hex(const std::string & data) { Sha256 s; s.update(data.data(), data.size()); return s.hex(); }

std::string sha256_file(const std::string & path) {
    FILE * f = std::fopen(path.c_str(), "rb");
    if (!f) return "";
    std::setvbuf(f, nullptr, _IOFBF, 16 << 20);  // the console's libc reads in buffer-sized pieces
    Sha256 s;
    std::vector<char> buf(8 << 20);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) s.update(buf.data(), n);
    std::fclose(f);
    return s.hex();
}

std::vector<ModelFile> parse_catalog(const std::string & text, std::string & err) {
    std::vector<ModelFile> out;
    std::istringstream in(text);
    std::string line;
    for (int no = 1; std::getline(in, line); no++) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream w(line);
        ModelFile f;
        std::string size;
        if (!(w >> f.group >> f.sha256 >> size >> f.repo >> f.rev >> f.path >> f.dest >> f.licence)) {
            err = "models.txt line " + std::to_string(no) + ": 8 fields expected";
            return {};
        }
        f.size = std::stoll(size);
        out.push_back(f);
    }
    return out;
}

std::string hf_url(const ModelFile & f) { return "https://huggingface.co/" + f.repo + "/resolve/" + f.rev + "/" + f.path; }

std::vector<GroupState> catalog_state(const std::vector<ModelFile> & files, const std::string & root) {
    std::vector<GroupState> out;
    for (const auto & f : files) {
        auto it = std::find_if(out.begin(), out.end(), [&](const GroupState & g) { return g.group == f.group; });
        if (it == out.end()) { out.push_back({f.group, 0, 0, true}); it = out.end() - 1; }
        const std::string p = root + "/" + f.dest;
        const int64_t done = size_of(p), part = size_of(p + ".part");
        it->total += f.size;
        it->have += done == f.size ? f.size : std::max<int64_t>(part, 0);
        if (done != f.size) it->installed = false;
    }
    return out;
}

int64_t free_bytes(const std::string & path) {
    struct statvfs s;
    return statvfs(path.c_str(), &s) == 0 ? (int64_t) s.f_bavail * (int64_t) s.f_frsize : -1;
}

bool download_file(const ModelFile & f, const std::string & root, const std::string & url, const Fetch & fetch,
                   int64_t free, const std::function<bool(int64_t, int64_t)> & progress, std::string & err) {
    err.clear();
    const std::string dest = root + "/" + f.dest, part = dest + ".part";
    mkdirs(dest.substr(0, dest.find_last_of('/')));
    if (size_of(dest) == f.size) return true;
    int64_t have = std::max<int64_t>(size_of(part), 0);
    if (have > f.size) { std::remove(part.c_str()); have = 0; }
    const int64_t missing = f.size - have;
    if (free >= 0 && free < missing) {
        err = "espaço insuficiente: faltam " + gb(missing - free) + " (o arquivo precisa de " + gb(missing) + " livres)";
        return false;
    }
    FILE * out = std::fopen(part.c_str(), "ab");
    if (!out) { err = "não consegui gravar " + part + ": " + std::strerror(errno); return false; }
    std::setvbuf(out, nullptr, _IOFBF, 8 << 20);
    bool cancelled = false, write_failed = false;
    int64_t done = have, total = f.size;
    const bool ok = fetch(url, have, [&](const char * p, size_t n) {
        if (std::fwrite(p, 1, n, out) != n) { write_failed = true; return false; }
        done += (int64_t) n;
        if (!progress(done, f.size)) { cancelled = true; return false; }
        return true;
    }, total, err);
    std::fclose(out);
    if (cancelled) { err = "cancelado"; return false; }
    if (write_failed) { err = "falha ao gravar no disco (cheio?)"; return false; }
    if (!ok) return false;
    const int64_t got = size_of(part);
    if (got != f.size) {
        if (got > f.size) std::remove(part.c_str());
        err = "download incompleto (" + gb(got) + " de " + gb(f.size) + "); tente de novo para continuar";
        return false;
    }
    if (sha256_file(part) != f.sha256) {
        std::remove(part.c_str());
        err = "o arquivo baixado não confere (sha256 diferente); foi apagado, tente de novo";
        return false;
    }
    if (std::rename(part.c_str(), dest.c_str()) != 0) { err = "não consegui renomear " + part; return false; }
    return true;
}

}  // namespace mg
