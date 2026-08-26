#include "clipboard.h"
#include <sol/state.hpp>
#include <sol/table.hpp>
#include <string>
#if defined(__APPLE__)
#include "platform/darwin/clipboard.h"
#elif defined(_WIN32)
#include "platform/win/clipboard.h"
#elif defined(__linux__)
#include "platform/linux/clipboard.h"
#else
#error "Unsupported platform — no clipboard implementation available"
#endif
#include "forward.h"
#include "modules/thread_kind.h"

namespace coconut::modules {

  void init_clipboard(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();
    sol::table cb      = lua.create_table();

    if (kind == ThreadKind::Main) {
      cb.set_function("readText", []() -> std::string { return clipboard::platformReadText(); });
      cb.set_function("writeText", [](const std::string& text) -> bool {
        return clipboard::platformWriteText(text);
      });
    } else {
      // Background — forward onto the main run loop and block.
      cb.set_function("readText", []() -> std::string {
        return forwardToMain([]() -> std::string { return clipboard::platformReadText(); });
      });
      cb.set_function("writeText", [](const std::string& text) -> bool {
        return forwardToMain([text]() -> bool { return clipboard::platformWriteText(text); });
      });
    }

    coconut["clipboard"] = cb;
  }

}  // namespace coconut::modules
