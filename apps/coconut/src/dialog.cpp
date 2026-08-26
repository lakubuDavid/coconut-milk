/// Platform dispatcher — routes dialog calls to the correct OS implementation
/// at compile time based on OS macros.
///
/// This is the central error boundary: any exception escaping a platform
/// implementation is converted to an unexpected(Error) so exceptions never
/// propagate into Lua bindings or the bridge.

#include "dialog.h"
#include "debug.h"

#include <exception>
#include <format>

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
  namespace {

    /// Wrap a platform call: catch everything, log, convert to Error.
    template <typename F>
    std::expected<Result, Error> guarded(const char* api, F&& fn) {
      try {
        return fn();
      } catch (const std::exception& e) {
        debug::error(std::format("dialog::{} exception: {}", api, e.what()));
        return std::unexpected(
            Error{
                .code    = ErrorCode::DialogError,
                .message = std::format("native dialog '{}' failed", api),
                .details = e.what(),
            }
        );
      } catch (...) {
        debug::error(std::format("dialog::{} unknown exception", api));
        return std::unexpected(
            Error{
                .code    = ErrorCode::DialogError,
                .message = std::format("native dialog '{}' failed with unknown exception", api),
            }
        );
      }
    }

  }  // namespace

  std::expected<Result, Error> messageBox(
      const std::string& title, const std::string& message, const std::string& kind
  ) {
    return guarded("messageBox", [&] { return platformMessageBox(title, message, kind); });
  }

  std::expected<Result, Error> openFile(
      const std::string& title, const std::vector<Filter>& filters, bool multi, bool chooseDir
  ) {
    return guarded("openFile", [&] { return platformOpenFile(title, filters, multi, chooseDir); });
  }

  std::expected<Result, Error> saveFile(
      const std::string& title, const std::string& defaultName, const std::vector<Filter>& filters
  ) {
    return guarded("saveFile", [&] { return platformSaveFile(title, defaultName, filters); });
  }

}  // namespace coconut::dialog
