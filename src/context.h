#ifndef CONTEXT_H
#define CONTEXT_H

#include "config.h"
#include "error.h"

#include <expected>
#include <sol/sol.hpp>
#include <string>

#include "window.h"

namespace coconut {

  struct App;

  namespace bridge {
    struct State;
  }  // namespace bridge

  namespace commands {
    struct Registry;
  }  // namespace commands

  namespace lua {
    struct Runtime;
  }  // namespace lua

  /// Window size used during startup and resize operations.
  struct CoconutWindowSize {
    int w = 0;
    int h = 0;
  };

  /// Forward declaration — defined below.
  struct CoconutWindowHandle;

  /// Runtime context exposed to Lua as `ctx`.
  struct CoconutContext {
    Config*             configs      = nullptr;
    App*                app          = nullptr;
    bridge::State*      bridge_state = nullptr;
    commands::Registry* commands     = nullptr;
    lua::Runtime*       lua_state    = nullptr;
    window::Window*     window       = nullptr;
    CoconutWindowHandle* window_handle = nullptr;

    /// Startup: selects which browser backend WebUI should use. Chainable.
    CoconutContext* setBrowser(const std::string& mode);

    /// Startup: initial window size. Chainable.
    CoconutContext* setWindowSize(const CoconutWindowSize& size);

    /// Startup: selects initial view by name. Chainable.
    CoconutContext* setInitialView(const std::string& name);

    /// Runtime: switch view by name.
    void show(const std::string& name);
    void reload();
    void close();

    /// Registers a single command handler (one command name => one handler).
    void bind(const std::string& name, sol::protected_function fn);

    /// Sends an event to the frontend asynchronously.
    void emit(const std::string& name, sol::object payload);

    /// Sends an event to the frontend synchronously.
    void emit_sync(const std::string& name, sol::object payload);
  };

  /// Window handle exposed to Lua as `ctx.window`.
  /// Wraps window-level operations (minimize, maximize, fullscreen, etc.).
  struct CoconutWindowHandle {
    App* app = nullptr;

    void show(const std::string& name);
    void reload();
    void close();
    void minimize();
    void maximize();
    void setFullscreen(bool on);
    void toggleFullscreen();
    void resize(int w, int h);
  };

  namespace context {

    /// Allocate a CoconutContext instance bound to a shared Config.
    std::expected<CoconutContext*, Error> create(Config* config);

    /// Destroy a CoconutContext instance.
    void destroy(CoconutContext* ctx);

  }  // namespace context

}  // namespace coconut

#endif  // CONTEXT_H
