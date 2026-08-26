/// Win32 native dialog stubs — not yet implemented.
///
/// Return unexpected(NotImplementedYet) so callers (Lua bindings included)
/// can distinguish "not available on this platform" from a cancelled dialog.

#include "dialog.h"
#include "../../debug.h"

namespace coconut::dialog {

  std::expected<Result, Error> platformMessageBox(
      const std::string& title, const std::string& message, const std::string& kind
  ) {
    (void)title;
    (void)message;
    (void)kind;
    debug::warn("Win32 dialog::messageBox() not yet implemented");
    return std::unexpected(
        Error{
            .code    = ErrorCode::NotImplementedYet,
            .message = "Win32 dialog::messageBox() is not implemented",
        }
    );
  }

  std::expected<Result, Error> platformOpenFile(
      const std::string& title, const std::vector<Filter>& filters, bool multi, bool chooseDir
  ) {
    (void)title;
    (void)filters;
    (void)multi;
    (void)chooseDir;
    debug::warn("Win32 dialog::openFile() not yet implemented");
    return std::unexpected(
        Error{
            .code    = ErrorCode::NotImplementedYet,
            .message = "Win32 dialog::openFile() is not implemented",
        }
    );
  }

  std::expected<Result, Error> platformSaveFile(
      const std::string& title, const std::string& defaultName, const std::vector<Filter>& filters
  ) {
    (void)title;
    (void)defaultName;
    (void)filters;
    debug::warn("Win32 dialog::saveFile() not yet implemented");
    return std::unexpected(
        Error{
            .code    = ErrorCode::NotImplementedYet,
            .message = "Win32 dialog::saveFile() is not implemented",
        }
    );
  }

}  // namespace coconut::dialog
