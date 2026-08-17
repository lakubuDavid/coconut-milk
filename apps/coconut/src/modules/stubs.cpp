// SPDX-License-Identifier: MIT
#include "stubs.h"
#include "context.h"

#include <string>

namespace coconut::modules {

void init_stubs(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  // These stubs are identical on both threads (they're just placeholders
  // that get overridden when main.lua runs).  Background thread still
  // sees them for safety, but the real overrides happen on the main
  // thread's Lua state.

  // ── config(ctx) — identity stub ─────────────────────────────
  coconut.set_function("config",
      [](CoconutContext* ctx) -> CoconutContext* { return ctx; });

  // ── views() — returns empty table ───────────────────────────
  coconut.set_function("views",
      [](sol::this_state s) -> sol::table {
        return sol::state_view(s).create_table();
      });

  // ── events(event) — no-op ──────────────────────────────────
  coconut.set_function("events",
      [](sol::object) { });
}

} // namespace coconut::modules
