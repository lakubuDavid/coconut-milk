#include "exec_command.h"
#include "common.h"
#include "debug.h"

#include <format>

namespace coconut::core {

  CommandResult execCommand(
      sol::state_view                                                 lua,
      const std::unordered_map<std::string, sol::protected_function>& handlers,
      const std::string&                                              command,
      const nlohmann::json&                                           args,
      CoconutContext*                                                 context
  ) {
    auto it = handlers.find(command);
    if (it == handlers.end()) {
      debug::error(std::format("command not found : {}", command));
      return CommandResult{
          .ok   = false,
          .data = {{"code", "CommandNotFound"}, {"message", "No handler for '" + command + "'"}},
      };
    }

    // Convert JSON args → sol::table
    sol::table params = common::toTable(lua, args);

    // ── Invoke the handler ────────────────────────────────────────
    sol::protected_function_result result;
    if (context) {
      result = it->second(params, context);
    } else {
      result = it->second(params);
    }

    // ── Serialize result → JSON ───────────────────────────────────
    if (result.valid()) {
      sol::object    val = result;
      nlohmann::json data;
      if (!val.valid() || val == sol::lua_nil) {
        data = nullptr;
      } else if (val.is<std::string>()) {
        data = val.as<std::string>();
      } else if (val.is<int>()) {
        data = val.as<int>();
      } else if (val.is<long long>()) {
        data = val.as<long long>();
      } else if (val.is<double>()) {
        data = val.as<double>();
      } else if (val.is<float>()) {
        data = static_cast<double>(val.as<float>());
      } else if (val.is<bool>()) {
        data = val.as<bool>();
      } else if (val.is<sol::table>()) {
        data = common::toJson(val.as<sol::table>());
      } else {
        data = nullptr;
      }

      // ── GC: reclaim temporary tables/strings ────────────────────
      lua_gc(lua.lua_state(), LUA_GCCOLLECT, 0);

      return CommandResult{.ok = true, .data = std::move(data)};
    }

    // Handler failed
    sol::error err = result;
    debug::warn(std::format("execCommand '{}': {}", command, err.what()));

    // ── GC even on error ──────────────────────────────────────────
    lua_gc(lua.lua_state(), LUA_GCCOLLECT, 0);

    return CommandResult{
        .ok   = false,
        .data = {{"code", "LuaError"}, {"message", err.what()}},
    };
  }

}  // namespace coconut::core
