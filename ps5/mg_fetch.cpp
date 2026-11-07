#include "mg_fetch.hpp"

#include "https_client.hpp"

namespace mg {

Fetch console_fetch() {
    return [](const std::string & url, int64_t from, const std::function<bool(const char *, size_t)> & sink, int64_t & total,
              std::string & err) {
        static std::string init_err;
        static const bool inited = ps5sd::https_init(init_err);
        if (!inited) { err = "HTTPS: " + init_err; return false; }
        ps5sd::HttpClient client;
        client.set_timeout(60);
        int  status = 0;
        bool range_ignored = false;
        auto headers_for = [&](const ps5sd::Url &) {
            ps5sd::HttpHeaders h{{"User-Agent", "PAUL3D"}};
            if (from > 0) h.emplace_back("Range", "bytes=" + std::to_string(from) + "-");
            return h;
        };
        auto on_response = [&](const ps5sd::HttpResponse & r) {
            status = r.status;
            if (from > 0 && r.status == 200) { range_ignored = true; return false; }  // would append the file again
            if (r.status != 200 && r.status != 206) return false;
            total = r.content_length >= 0 ? from + r.content_length : -1;
            return true;
        };
        ps5sd::HttpResponse resp;
        std::string final_url;
        const bool ok = ps5sd::http_get_following(client, url, headers_for, on_response, sink, resp, final_url, err);
        if (range_ignored) { err = "o servidor não aceitou continuar o download"; return false; }
        if (ok && status != 200 && status != 206) { err = "HTTP " + std::to_string(status); return false; }
        return ok;
    };
}

}  // namespace mg
