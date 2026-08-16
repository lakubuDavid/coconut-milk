#ifndef COCONUT_MODULES_FS_H
#define COCONUT_MODULES_FS_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.fs.* (thread-safe, same on both threads).
void init_fs(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
