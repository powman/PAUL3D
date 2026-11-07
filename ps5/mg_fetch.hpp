// The console's Fetch for mg_download: PS5SD's HTTPS client (mbedTLS, the Mozilla CA list), redirects followed
// (Hugging Face sends the bytes from its CDN), a Range header to resume. Console build only (scripts/build-mg.sh).
#pragma once
#include "mg_download.hpp"

namespace mg {
Fetch console_fetch();
}
