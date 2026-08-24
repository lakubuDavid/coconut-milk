#include "clipboard.h"
#include <sol/state.hpp>
#include <sol/table.hpp>
#include <string>
#include "../packages/clipboard.h"  // C++ clipboard namespace
#include "forward.h"
#include "modules/thread_kind.h"

namespace coconut::modules {

  void init_clipboard(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();
    sol::table cb      = lua.create_table();

    if (kind == ThreadKind::Main) {
      cb.set_function("readText", []() -> std::string { return coconut::clipboard::readText(); });
      cb.set_function("writeText", [](const std::string& text) -> bool {
        return coconut::clipboard::writeText(text);
      });
    } else {
      // Background — forward onto the main run loop and block.
      cb.set_function("readText", []() -> std::string {
        return forwardToMain([]() -> std::string { return coconut::clipboard::readText(); });
      });
      cb.set_function("writeText", [](const std::string& text) -> bool {
        return forwardToMain([text]() -> bool { return coconut::clipboard::writeText(text); });
      });
    }

    coconut["clipboard"] = cb;
  }

}  // namespace coconut::modules
