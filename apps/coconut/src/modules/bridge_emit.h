// SPDX-License-Identifier: MIT
#pragma once

#include "thread_kind.h"

#include <sol/state.hpp>

namespace coconut::modules {

/// Register `_bridge_emit` and `_js_log` helper functions on the
/// `coconut` table.  These are low-level bridge primitives:
///
///   - `_bridge_emit(name, payloadJson)` — forwards a JSON string payload
///     to the JS bridge as a named event.
///   - `_js_log(entry)` — receives JS-side console.error/warn/info and
///     routes it to the C++ debug logging system.
///
void init_bridge_emit(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules
