#ifndef COCONUT_BG_THREAD_H
#define COCONUT_BG_THREAD_H

#include "commands.h"
#include "config.h"
#include "context.h"
#include "dispatch.h"
#include "error.h"

#include <sol/sol.hpp>

#include <atomic>
#include <expected>
#include <thread>

namespace coconut {

struct App;

namespace bg_thread {

/// Background thread state — owns a separate Lua VM for running commands.
///
/// The background thread runs user commands (the default for ctx:bind) so
/// that CPU-heavy or I/O-bound Lua code does not block the main thread
/// which owns the webview, bridge, and platform APIs.
///
/// Two lock-free SPSC Outboxes connect the threads:
///   inbox  (Main→Bg) — main thread pushes CommandCall messages,
///                       background thread pops and executes them.
///   outbox (Bg→Main) — background thread pushes CommandResult messages,
///                       main thread drains them alongside its own outbox.
struct Context {
  /// Owning App — set during create(). The App outlives the bg thread.
  App* app = nullptr;

  /// Whether the background thread should keep running.
  std::atomic<bool> running{false};

  /// The background thread handle. Joined during stop().
  std::thread thread;

  /// Background thread's Lua state (separate from main thread's).
  sol::state* lua_state = nullptr;

  /// Background thread's CoconutContext (separate from main thread's).
  CoconutContext* ctx = nullptr;

  /// Background thread's command registry.
  commands::Registry* commands = nullptr;

  /// Inbox: messages from main thread → background thread.
  /// Producer: main thread.  Consumer: background thread (its run loop).
  dispatch::Outbox inbox;

  /// Outbox: results from background thread → main thread.
  /// Producer: background thread.  Consumer: main thread (via dispatch::drain).
  dispatch::Outbox outbox;
};

/// Allocate and initialize the background thread context.
/// Loads .g.lua files into the background Lua state.
/// Does NOT start the thread — call start() after setup.
std::expected<Context*, Error> create(App* app, Config* config);

/// Destroy the background thread context.
/// Must be called after stop().
void destroy(Context* bg);

/// Start the background thread (enters run loop).
void start(Context* bg);

/// Signal the background thread to stop and wait for it to finish.
void stop(Context* bg);

}  // namespace bg_thread
}  // namespace coconut

#endif  // COCONUT_BG_THREAD_H
