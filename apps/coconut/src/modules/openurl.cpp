#include "openurl.h"
#include "debug.h"
#include "../packages/open_url.h"  // C++ open_url namespace

namespace coconut::modules {

void init_openurl(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  if (kind == ThreadKind::Main) {
    coconut.set_function("openUrl", [](const std::string& url) -> bool {
      return coconut::open_url::open(url);
    });
  } else {
    coconut.set_function("openUrl", [](const std::string&) -> bool {
      debug::warn("[bg] coconut.openUrl requires the main thread");
      return false;
    });
  }
}

} // namespace coconut::modules
