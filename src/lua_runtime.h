#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H

#include "config.h"
#include "context.h"
#include "error.h"

#include <expected>
#include <lua.h>
#include <sol/sol.hpp>
#include <sol/state.hpp>

namespace coconut {
  namespace lua {

    /// Lua runtime wrapper for sol2.
    struct Runtime {
      Config*         configs   = nullptr;
      CoconutContext* context   = nullptr;
      App*            app       = nullptr;
      sol::state*     lua_state = nullptr;
    };

    /// Create Lua runtime and install the Coconut API.
    std::expected<Runtime*, Error> create(Config* config, CoconutContext* ctx);

    /// Destroy Lua runtime and its sol2 state.
    void destroy(Runtime* runtime);

    void _bindCoconutLuaApi(Runtime* runtime);
    void _bindViewClass(Runtime* runtime);
    void _bindUserType(Runtime* runtime);
    void _registerBuiltinCommands(Runtime* runtime);

    /// Load the application entry point (main.lua) and execute the
    /// coconut.config(ctx) callback if present.
    ///
    /// The ctx setters (setWindowSize, setInitialView) mutate the
    /// shared Config in-place, merging app-level overrides on top of
    /// config-file defaults.  If the callback returns a table, additional
    /// scalar fields (window_width, ...) and views from that table
    /// are also merged into cfg.
    ///
    /// Returns true if main.lua was loaded and processed.
    /// Returns false if main.lua does not exist (not an error).
    /// Returns an Error if the file exists but loading or cb execution fails.
    std::expected<bool, Error> loadEntryPoint(Runtime* runtime, Config* cfg);

    /// Wire the ctx.window Lua binding after the runtime's app pointer is set.
    /// Must be called after runtime->app is wired (i.e. after create() and
    /// after lua_runtime->app = app).
    void wireWindowHandle(Runtime* runtime);

    /// Invoke a view descriptor lifecycle callback (on_load, on_mount, on_unmount).
    /// Looks up the view descriptor in the Lua registry and calls the stored
    /// callback if it exists.  on_load is only called once per view.
    void invokeViewCallback(Runtime* runtime,
                            const std::string& viewName,
                            const std::string& eventName);

    /// Call a registered Lua command by name.
    ///
    /// Looks up the handler previously registered via ctx:bind(name, fn) and
    /// invokes it with (params, ctx).  params is a Lua table (the payload).
    ///
    /// Returns the value returned by the handler on success.
    /// Returns an Error if the command is not registered or the call fails.
    std::expected<sol::object, Error> call(Runtime* runtime,
                                           const std::string& name,
                                           sol::table params);

  }  // namespace lua
}  // namespace coconut

#endif  // LUA_RUNTIME_H
