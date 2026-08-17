#ifndef COCONUT_MODULES_CLIPBOARD_H
#define COCONUT_MODULES_CLIPBOARD_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.clipboard.readText / .writeText.
/// On Background, registers stubs.
void init_clipboard(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
