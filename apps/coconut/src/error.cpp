#include "error.h"

namespace coconut {

  const char* toString(ErrorCode code) {
    switch (code) {
      // ── General ────────────────────────────────────────────────────
      case ErrorCode::Ok:
        return "ok";
      case ErrorCode::Unknown:
        return "unknown";
      case ErrorCode::NotFound:
        return "not_found";
      case ErrorCode::NotImplementedYet:
        return "not_implemented_yet";

      // ── Config & views ─────────────────────────────────────────────
      case ErrorCode::InvalidConfig:
        return "invalid_config";
      case ErrorCode::InvalidView:
        return "invalid_view";
      case ErrorCode::MissingFile:
        return "missing_file";
      case ErrorCode::ParseError:
        return "parse_error";

      // ── Commands & runtime ─────────────────────────────────────────
      case ErrorCode::DuplicateCommand:
        return "duplicate_command";
      case ErrorCode::CommandNotFound:
        return "command_not_found";
      case ErrorCode::InvalidPayload:
        return "invalid_payload";
      case ErrorCode::NotReady:
        return "not_ready";
      case ErrorCode::QueueOverflow:
        return "queue_overflow";
      case ErrorCode::LuaError:
        return "lua_error";

      // ── Bridge & webview ───────────────────────────────────────────
      case ErrorCode::BridgeError:
        return "bridge_error";
      case ErrorCode::WebViewError:
        return "webview_error";

      // ── Platform ───────────────────────────────────────────────────
      case ErrorCode::IoError:
        return "io_error";
      case ErrorCode::DialogError:
        return "dialog_error";
      case ErrorCode::WindowError:
        return "window_error";
    }
    return "unknown";
  }

}  // namespace coconut
