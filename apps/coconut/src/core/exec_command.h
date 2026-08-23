#ifndef CORE_EXEC_COMMAND_H
#define CORE_EXEC_COMMAND_H

/// @file exec_command.h
///
/// Shared command execution helper used by both the worker (bg thread)
/// and the bridge (main thread).  Converts JSON args → sol::table,
/// invokes the Lua handler, and serializes the result back to JSON.
///
/// Thread-safe as long as the caller provides a sol::state_view that
/// belongs to the calling thread.  A full GC pass runs after every
/// call to reclaim temporary objects.

#include "context.h"
#include "core/messages.h"  // CommandResult
#include "modules/json.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include <string>
#include <unordered_map>

namespace coconut::core {

  /// Execute a named command using the given handler map.
  ///
  /// Each call runs inside a temporary sandboxed environment so that
  /// accidental global assignments (e.g. `x = 42` instead of `local x = 42`)
  /// do not leak into the shared Lua state.  A full GC pass runs after
  /// every call to reclaim temporary objects.
  ///
  /// @param lua       sol::state_view for the calling thread
  /// @param handlers  map of command_name → sol::protected_function
  /// @param command   command name to look up
  /// @param args      JSON arguments to convert to sol::table
  /// @param context   optional CoconutContext pointer passed to the handler
  ///
  /// @returns CommandResult with ok=true/data=result on success,
  ///          or ok=false/data={code, message} on failure.
  CommandResult execCommand(
      sol::state_view                                                 lua,
      const std::unordered_map<std::string, sol::protected_function>& handlers,
      const std::string&                                              command,
      const nlohmann::json&                                           args,
      CoconutContext*                                                 context = nullptr
  );

}  // namespace coconut::core

#endif  // CORE_EXEC_COMMAND_H
