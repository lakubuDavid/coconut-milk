#ifndef APP_H
#define APP_H

#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "core/bridge.h"
#include "core/dispatcher.h"
#include "dispatch.h"
#include "error.h"
#include "fs.h"
#include "main_runtime.h"
#include "window.h"

#include <webview/webview.h>

#include <expected>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace coconut {

  /// Top-level runtime owner.
  struct App {
    Config*         configs = nullptr;
    CoconutContext* context = nullptr;

    /// webview handle (created by App core).
    webview_t webview = nullptr;

    window::Window*     window       = nullptr;
    lua::Runtime*       lua_state    = nullptr;
    bridge::State*      bridge_state = nullptr;
    commands::Registry* commands     = nullptr;
    fs::Roots*          fs           = nullptr;
    std::vector<Error>  errors;

    /// Platform-level keybind combos (consumed by NSEvent monitor before
    /// reaching the webview). Populated by Lua coconut.keybind() with
    /// opts.platform=true.
    std::unordered_set<std::string> platform_keybinds;

    /// Per-worker CoconutContexts (one per core Worker; destroyed in
    /// app::destroy after the worker pool has shut down).
    std::vector<CoconutContext*> worker_contexts;

    /// Core message architecture (Phase 1 wiring — constructed alongside the
    /// legacy path; inbound traffic still flows through the legacy handlers).
    std::unique_ptr<core::Dispatcher> dispatcher;
    std::unique_ptr<core::Bridge>     core_bridge;
  };

  namespace app {

    /// Allocate an App. Does not own/destroy the shared Config.
    /// If nativeWindow is provided (non-null), uses that window instead of creating a new one.
    std::expected<App*, Error> create(Config* configs, void* nativeWindow = nullptr);

    /// Destroy the App and owned runtime submodules.
    /// Does not destroy the shared Config.
    void destroy(App* app);

    void setConfigs(App* app, Config* cfg);

    std::optional<Error> getError(App* app);
    void                 pushError(App* app, Error err);
    void pushError(App* app, ErrorCode code, std::string message, std::string details);

    void run(App* app);

  }  // namespace app

}  // namespace coconut

#endif  // APP_H
