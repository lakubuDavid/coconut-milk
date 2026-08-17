#ifndef COCONUT_NOTIFY_H
#define COCONUT_NOTIFY_H

#include <string>

namespace coconut::notify {

/// Show a system notification.
/// Returns true on success.
bool notify(const std::string& title, const std::string& body);

} // namespace coconut::notify

#endif // COCONUT_NOTIFY_H