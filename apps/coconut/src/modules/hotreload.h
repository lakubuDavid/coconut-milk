#ifndef COCONUT_MODULES_HOTRELOAD_H
#define COCONUT_MODULES_HOTRELOAD_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.hotreload.
/// On Background, registers a stub.
void init_hotreload(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
