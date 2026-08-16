#ifndef COCONUT_MODULES_JSON_H
#define COCONUT_MODULES_JSON_H

#include "thread_kind.h"

#include <sol/sol.hpp>

#include <nlohmann/json.hpp>

namespace coconut::modules {

/// Register coconut.json.jsonify() and coconut.json.parse().
/// Thread-safe — same implementation on both threads.
void init_json(sol::state& lua, ThreadKind kind);

// ---------------------------------------------------------------------------
// JSON ↔ Lua conversion utilities (thread-safe)
// Moved from bridge.cpp so they're available to all modules without
// depending on the bridge layer.
// ---------------------------------------------------------------------------

/// Convert a nlohmann::json value to a Lua table/primitive.
sol::table toTable(sol::state_view lua, const nlohmann::json& json);

/// Parse a JSON string then convert to Lua.
sol::table toTable(sol::state_view lua, const std::string& jsonStr);

/// Convert a Lua table to a nlohmann::json value.
nlohmann::json toJson(const sol::table& table);

} // namespace coconut::modules

#endif
