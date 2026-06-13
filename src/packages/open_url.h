#ifndef COCONUT_OPEN_URL_H
#define COCONUT_OPEN_URL_H

#include <string>

namespace coconut::open_url {

/// Open the given URL in the system-default browser.
/// Returns true on success, false on failure.
bool open(const std::string& url);

} // namespace coconut::open_url

#endif // COCONUT_OPEN_URL_H