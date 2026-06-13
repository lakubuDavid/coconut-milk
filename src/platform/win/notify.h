#ifndef COCONUT_PLATFORM_WIN_NOTIFY_H
#define COCONUT_PLATFORM_WIN_NOTIFY_H

#include <string>

namespace coconut::notify {
bool platformNotify(const std::string& title, const std::string& body);
} // namespace coconut::notify

#endif // COCONUT_PLATFORM_WIN_NOTIFY_H