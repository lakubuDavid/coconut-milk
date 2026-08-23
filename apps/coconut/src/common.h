#ifndef COCONUT_COMMON_H
#define COCONUT_COMMON_H

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include <string>
#include <string_view>

namespace coconut::common {

  /// Escape a string for embedding in a JS/Lua quoted literal.
  /// \p quote_char  '\'' for JS single-quoted, '"' for Lua double-quoted.
  std::string escapeString(std::string_view s, char quote_char);

  // ── JSON ↔ Lua conversion utilities (thread-safe) ──────────────────

  sol::table     toTable(sol::state_view lua, const nlohmann::json& json);
  sol::table     toTable(sol::state_view lua, const std::string& jsonStr);
  nlohmann::json toJson(const sol::table& table);

}  // namespace coconut::common

#endif  // COCONUT_COMMON_H
