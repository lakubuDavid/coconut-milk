/// Stub: dialog platform functions.
/// Real declarations come from platform/darwin/dialog.h (or win/linux equivalent).
/// These definitions satisfy the linker in NO_PLATFORM builds.

#ifdef NO_PLATFORM

#include "dialog.h"
#include <string>
#include <vector>

namespace coconut::dialog {

Result platformMessageBox(const std::string& title,
                          const std::string& message,
                          const std::string& kind) {
  (void)title; (void)message; (void)kind;
  return Result{};
}

Result platformOpenFile(const std::string& title,
                        const std::vector<Filter>& filters,
                        bool multi,
                        bool chooseDir) {
  (void)title; (void)filters; (void)multi; (void)chooseDir;
  return Result{};
}

Result platformSaveFile(const std::string& title,
                        const std::string& defaultName,
                        const std::vector<Filter>& filters) {
  (void)title; (void)defaultName; (void)filters;
  return Result{};
}

} // namespace coconut::dialog

#endif // NO_PLATFORM
