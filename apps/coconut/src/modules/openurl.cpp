#include "openurl.h"
#include "../packages/open_url.h"  // C++ open_url namespace
#include "debug.h"
#include "forward.h"

namespace coconut::modules {

  void init_openurl(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();

    if (kind == ThreadKind::Main) {
      coconut.set_function("openUrl", [](const std::string& url) -> bool {
        return coconut::open_url::open(url);
      });
    } else {
      coconut.set_function("openUrl", [](const std::string& url) -> bool {
        // Forward onto the main run loop and block until opened.
        return forwardToMain([&]() -> bool { return coconut::open_url::open(url); });
      });
    }
  }

}  // namespace coconut::modules
