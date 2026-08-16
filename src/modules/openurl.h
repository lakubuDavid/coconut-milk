#ifndef COCONUT_MODULES_OPENURL_H
#define COCONUT_MODULES_OPENURL_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.openUrl.
/// On Background, registers a stub.
void init_openurl(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
