#ifndef COCONUT_MODULES_KEYBIND_H
#define COCONUT_MODULES_KEYBIND_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register `coconut.keybind(combo, handler, opts?)` and
/// `coconut.__registerPlatformKeybind(params)`.
///
/// The keybind system implements a hybrid chain:
///   - Platform-level keybinds registered via `__registerPlatformKeybind`
///     go into the App's `platform_keybinds` set and are handled natively.
///   - Lua-level keybinds registered via `coconut.keybind()` are stored in
///     `coconut._keybinds[combo]` and dispatched during bridge message handling.
///
/// Both return an unregister function.
void init_keybind(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
