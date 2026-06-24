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

  /// Screen position / offset used for window positioning.
  struct CoconutPoint {
    int x = 0;
    int y = 0;
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

    /// Startup: initial window size. Chainable.
    CoconutContext* setWindowSize(const CoconutWindowSize& size);

    /// Startup: minimum window size. Chainable.
    CoconutContext* setMinimumWindowSize(const CoconutWindowSize& size);
    /// Startup: maximum window size. Chainable.
    CoconutContext* setMaximumWindowSize(const CoconutWindowSize& size);
    /// Startup: minimum window width. Chainable.
    CoconutContext* setMinimumWindowWidth(int w);
    /// Startup: minimum window height. Chainable.
    CoconutContext* setMinimumWindowHeight(int h);
    /// Startup: maximum window width. Chainable.
    CoconutContext* setMaximumWindowWidth(int w);
    /// Startup: maximum window height. Chainable.
    CoconutContext* setMaximumWindowHeight(int h);

    /// Startup: window title. Chainable.
    CoconutContext* setTitle(const std::string& t);

    /// Startup: whether the window is resizable by the user. Chainable.
    CoconutContext* setResizable(bool on);

    /// Startup: frameless (no native titlebar). Chainable.
    CoconutContext* setFrameless(bool on);

    /// Startup: transparent window background. Chainable.
    CoconutContext* setTransparent(bool on);

    /// Startup: selects initial view by name. Chainable.
    CoconutContext* setInitialView(const std::string& name);

    /// Runtime: switch view by name.
    void show(const std::string& name);
    void reload();
    void close();

    /// Registers a single command handler on the background thread (default).
    void bind(const std::string& name, sol::protected_function fn);

    /// Registers a single command handler on the main thread.
    /// Use for commands that interact with webview, dialogs, or other
    /// platform APIs that must run on the main thread.
    void bind_mt(const std::string& name, sol::protected_function fn);

    /// Re-registers a command handler, overwriting any existing one.
    /// Unlike bind(), this does not throw on duplicates.
    void rebind(const std::string& name, sol::protected_function fn);

    /// Dispatches an event through the three-tier chain and forwards to JS.
    /// Accepts a single event table with a 'name' field.
    void emit(sol::table event);

    /// Synchronous version of emit().
    void emit_sync(sol::table event);
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
    void setMovableByBackground(bool on);
    /// Set window background color (0-1 range).
    void setBackgroundColor(float r, float g, float b, float a = 1.0f);
    /// Set absolute screen position (bottom-left origin — macOS native).
    void setPosition(int x, int y);
    /// Move window by offset (dx = right, dy = up in screen coords).
    void move(const CoconutPoint& offset);
    /// Get current screen position {x, y}.
    CoconutPoint getPosition();
  };

  namespace context {

    /// Allocate a CoconutContext instance bound to a shared Config.
    std::expected<CoconutContext*, Error> create(Config* config);

    /// Destroy a CoconutContext instance.
    void destroy(CoconutContext* ctx);

  }  // namespace context

}  // namespace coconut

#endif  // CONTEXT_H
