#ifndef COCONUT_MODULES_LOG_H
#define COCONUT_MODULES_LOG_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.log / .info / .warn / .error (thread-safe, same on both threads).
void init_log(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
