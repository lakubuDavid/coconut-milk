#ifndef ERROR_H
#define ERROR_H

#include <string>

namespace coconut {

enum class ErrorCode {
  Ok,
  Unknown,
  InvalidConfig,
  InvalidView,
  MissingFile,
  DuplicateCommand,
  LuaError,
  BridgeError,
  WebUiError,
};

struct Error {
  ErrorCode code = ErrorCode::Ok;
  std::string message;
  std::string details;
};

} // namespace coconut

#endif // ERROR_H
