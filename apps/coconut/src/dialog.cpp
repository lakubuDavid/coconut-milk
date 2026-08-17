/// Platform dispatcher — routes dialog calls to the correct OS implementation
/// at compile time based on OS macros.

#include "dialog.h"

#if defined(__APPLE__)
  #include "platform/darwin/dialog.h"
#elif defined(_WIN32)
  #include "platform/win/dialog.h"
#elif defined(__linux__)
  #include "platform/linux/dialog.h"
#else
  #error "Unsupported platform — no dialog implementation available"
#endif

namespace coconut::dialog {

Result messageBox(const std::string& title,
                  const std::string& message,
                  const std::string& kind) {
  return platformMessageBox(title, message, kind);
}

Result openFile(const std::string& title,
                const std::vector<Filter>& filters,
                bool multi,
                bool chooseDir) {
  return platformOpenFile(title, filters, multi, chooseDir);
}

Result saveFile(const std::string& title,
                const std::string& defaultName,
                const std::vector<Filter>& filters) {
  return platformSaveFile(title, defaultName, filters);
}

} // namespace coconut::dialog
