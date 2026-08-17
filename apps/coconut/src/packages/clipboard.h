#ifndef COCONUT_CLIPBOARD_H
#define COCONUT_CLIPBOARD_H

#include <string>

namespace coconut::clipboard {

/// Read plain text from the system clipboard.
/// Returns empty string if no text is available.
std::string readText();

/// Write plain text to the system clipboard.
/// Returns true on success.
bool writeText(const std::string& text);

} // namespace coconut::clipboard

#endif // COCONUT_CLIPBOARD_H