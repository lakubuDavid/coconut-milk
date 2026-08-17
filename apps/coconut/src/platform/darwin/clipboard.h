#ifndef COCONUT_PLATFORM_CLIPBOARD_H
#define COCONUT_PLATFORM_CLIPBOARD_H

#include <string>

namespace coconut::clipboard {
std::string platformReadText();
bool platformWriteText(const std::string& text);
} // namespace coconut::clipboard

#endif // COCONUT_PLATFORM_CLIPBOARD_H