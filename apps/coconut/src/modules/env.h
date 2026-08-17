#ifndef COCONUT_MODULES_ENV_H
#define COCONUT_MODULES_ENV_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.env (main thread only — no-op on background).
void init_env(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
