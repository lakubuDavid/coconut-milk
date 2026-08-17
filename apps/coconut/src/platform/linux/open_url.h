#ifndef COCONUT_PLATFORM_LINUX_OPEN_URL_H
#define COCONUT_PLATFORM_LINUX_OPEN_URL_H

#include <string>

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url);

inline bool open(const std::string& url) {
    return platformOpenUrl(url);
}

} // namespace coconut::open_url

#endif // COCONUT_PLATFORM_LINUX_OPEN_URL_H