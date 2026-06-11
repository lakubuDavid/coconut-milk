#ifndef ERROR_H
#define ERROR_H

#include <string>

namespace coconut {

  /// Machine-readable error categories.
  enum class ErrorCode {
    Ok,
    Unknown,
    InvalidConfig,
    InvalidView,
    MissingFile,
    DuplicateCommand,
    CommandNotFound,
    InvalidPayload,
    NotReady,
    QueueOverflow,
    LuaError,
    BridgeError,
    WebViewError,
    ParseError,
    IoError,
    NotImplementedYet
  };

  /// Shared error value type.
  ///
  /// Coconut uses these values with `std::expected<T, Error>` for
  /// recoverable failures.
  struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::string details;
  };

}  // namespace coconut

#endif  // ERROR_H
