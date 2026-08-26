#ifndef ERROR_H
#define ERROR_H

#include <string>

namespace coconut {

  /// Machine-readable error categories.
  ///
  /// Grouped by domain. Existing enumerators keep their order — append new
  /// ones at the end of their group, never reorder or remove.
  enum class ErrorCode {
    // ── General ────────────────────────────────────────────────────
    Ok,
    Unknown,
    NotFound,
    NotImplementedYet,

    // ── Config & views ─────────────────────────────────────────────
    InvalidConfig,
    InvalidView,
    MissingFile,
    ParseError,

    // ── Commands & runtime ─────────────────────────────────────────
    DuplicateCommand,
    CommandNotFound,
    InvalidPayload,
    NotReady,
    QueueOverflow,
    LuaError,

    // ── Bridge & webview ───────────────────────────────────────────
    BridgeError,
    WebViewError,

    // ── Platform ───────────────────────────────────────────────────
    IoError,
    DialogError,
    WindowError,
  };

  /// Stable snake_case name for an ErrorCode (e.g. "invalid_config",
  /// "dialog_error"). Used by bridge error envelopes and diagnostics so
  /// codes can cross the JS/Lua boundary as strings.
  const char* toString(ErrorCode code);

  /// Shared error value type.
  ///
  /// Coconut uses these values with `std::expected<T, Error>` for
  /// recoverable failures.
  struct Error {
    ErrorCode   code = ErrorCode::Ok;
    std::string message;
    std::string details;
  };

}  // namespace coconut

#endif  // ERROR_H
