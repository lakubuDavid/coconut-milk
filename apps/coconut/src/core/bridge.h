#ifndef CORE_BRIDGE_H
#define CORE_BRIDGE_H

#include "error.h"

#include <webview.h>
#include <nlohmann/json.hpp>
#include <sol/state_view.hpp>

#include <expected>
#include <memory>
#include <string>

namespace coconut::core {

  struct WorkerPool;  // forward decl

  class Bridge;  // forward decl — defined below

  /// Fluent builder for Bridge. Each `with*` step configures a dependency;
  /// `build()` validates and constructs the Bridge instance.
  struct BridgeBuilder {
    webview_t*      WebViewHandle = nullptr;
    sol::state_view LuaState      = nullptr;
    WorkerPool*     WorkerPoolPtr = nullptr;

    /// Bind the webview handle for outbound JS evaluation.
    BridgeBuilder& withWebView(webview_t* handle);
    /// Bind the main-thread Lua state for event dispatch and command routing.
    BridgeBuilder& withLuaState(sol::state_view lua);
    /// Bind the worker pool for background command execution.
    BridgeBuilder& withWorkerPool(WorkerPool* pool);
    /// Validate configuration and construct the Bridge.
    std::expected<std::unique_ptr<Bridge>, coconut::Error> build();
  };

  /// The bridge between C++ and the webview/Lua runtime.
  /// Created via `Bridge::builder().withWebView(...).withLuaState(...).build()`.
  class Bridge {
    friend struct BridgeBuilder;

   private:
    webview_t*      _WebViewHandle;
    sol::state_view _MainLuaState;
    WorkerPool*     _WorkerPool;

    Bridge(webview_t* webview, sol::state_view lua, WorkerPool* pool);

   public:
    /// Begin a fluent builder for a new Bridge.
    static BridgeBuilder builder();

    // ── Outbound: C++ → Lua / JS ──────────────────────────────────────

    /// Dispatch events through `coconut._dispatch` on the main Lua thread.
    void emitToLua(const std::string& name, const nlohmann::json& payload);

    /// Trigger `coconut.on` / webview event emission in the frontend.
    void emitToJS(const std::string& eventName, const nlohmann::json& payload);

    /// Forward a command call to the worker pool for background execution.
    void callLuaCommand(const std::string& name, const nlohmann::json& args);
  };

}  // namespace coconut::core

#endif  // CORE_BRIDGE_H
