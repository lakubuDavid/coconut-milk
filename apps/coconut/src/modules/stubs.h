#ifndef COCONUT_MODULES_STUBS_H
#define COCONUT_MODULES_STUBS_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register built-in stub functions on the `coconut` table that
/// are overridden by `main.lua` at startup:
///
///   - `coconut.config(ctx)` — returns ctx (identity).  Users override
///     this to apply app configuration.
///   - `coconut.views()` — returns an empty table.  Users override
///     this to declare named view descriptors.
///   - `coconut.events(event)` — no-op.  Users override this to
///     handle events at the app level (Tier 3 dispatch fallback).
///
void init_stubs(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
