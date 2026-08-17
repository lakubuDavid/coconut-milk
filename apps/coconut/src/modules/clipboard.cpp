#include "clipboard.h"
#include "debug.h"
#include "../packages/clipboard.h"  // C++ clipboard namespace

namespace coconut::modules {

void init_clipboard(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();
  sol::table cb = lua.create_table();

  if (kind == ThreadKind::Main) {
    cb.set_function("readText", []() -> std::string {
      return coconut::clipboard::readText();
    });
    cb.set_function("writeText", [](const std::string& text) -> bool {
      return coconut::clipboard::writeText(text);
    });
  } else {
    cb.set_function("readText", []() -> std::string {
      debug::warn("[bg] clipboard.readText requires the main thread");
      return {};
    });
    cb.set_function("writeText", [](const std::string&) -> bool {
      debug::warn("[bg] clipboard.writeText requires the main thread");
      return false;
    });
  }

  coconut["clipboard"] = cb;
}

} // namespace coconut::modules
