#ifndef COCONUT_MODULES_NOTIFY_H
#define COCONUT_MODULES_NOTIFY_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.notify.
/// On Background, registers a stub.
void init_notify(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
