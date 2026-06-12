/// Win32 native dialog stubs — not yet implemented.

#include "dialog.h"
#include "../../debug.h"

namespace coconut::dialog {

Result platformMessageBox(const std::string& title,
                          const std::string& message,
                          const std::string& kind) {
  debug::warn("Win32 dialog::messageBox() not yet implemented");
  return Result{};
}

Result platformOpenFile(const std::string& title,
                        const std::vector<Filter>& filters,
                        bool multi,
                        bool chooseDir) {
  (void)chooseDir;
  debug::warn("Win32 dialog::openFile() not yet implemented");
  return Result{};
}

Result platformSaveFile(const std::string& title,
                        const std::string& defaultName,
                        const std::vector<Filter>& filters) {
  debug::warn("Win32 dialog::saveFile() not yet implemented");
  return Result{};
}

} // namespace coconut::dialog
