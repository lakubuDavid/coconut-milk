#ifndef COCONUT_MODULES_BG_STUBS_H
#define COCONUT_MODULES_BG_STUBS_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register bg-only stubs: coconut.views(), coconut.config(), coconut.events(),
/// and coconut.__registerPlatformKeybind.
/// These are only registered on the background thread (no-op on Main).
void init_bg_stubs(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
