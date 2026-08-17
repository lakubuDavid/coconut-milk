#ifndef COCONUT_PLATFORM_OPEN_URL_H
#define COCONUT_PLATFORM_OPEN_URL_H

#include <string>

namespace coconut::open_url {

/// Platform implementation (declared first so inline open() can call it).
bool platformOpenUrl(const std::string& url);

/// Open a URL in the system's default handler (browser, mailto app, etc.).
/// Returns true on success.
inline bool open(const std::string& url) {
    return platformOpenUrl(url);
}

} // namespace coconut::open_url

#endif // COCONUT_PLATFORM_OPEN_URL_H