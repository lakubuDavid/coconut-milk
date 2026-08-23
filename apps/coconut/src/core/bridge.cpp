#include "bridge.h"
#include "app.h"
#include "common.h"
#include "debug.h"
#include "webview/types.h"
#include "worker.h"

#include <format>
#include <string>

namespace coconut::core {

  // ── BridgeBuilder ───────────────────────────────────────────────────

  BridgeBuilder& BridgeBuilder::withWebView(webview_t* handle) {
    WebViewHandle = handle;
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withLuaState(sol::state_view lua) {
    LuaState = lua;
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withWorkerPool(WorkerPool* pool) {
    WorkerPoolPtr = pool;
    return *this;
  }

  std::expected<std::unique_ptr<Bridge>, coconut::Error> BridgeBuilder::build() {
    if (!WebViewHandle) {
      return std::unexpected(coconut::Error{
          .code    = ErrorCode::InvalidConfig,
          .message = "BridgeBuilder::build: webview handle is null"});
    }

    if (!LuaState.lua_state()) {
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "BridgeBuilder::build: Lua state is null"});
    }

    return std::unique_ptr<Bridge>(new Bridge(WebViewHandle, LuaState, WorkerPoolPtr));
  }

  // ── Bridge ──────────────────────────────────────────────────────────

  BridgeBuilder Bridge::builder() {
    return BridgeBuilder{};
  }

  Bridge::Bridge(webview_t* webview, sol::state_view lua, WorkerPool* pool)
      : _WebViewHandle(webview), _MainLuaState(lua), _WorkerPool(pool) {
  }

  // ── Outbound: C++ → Lua / JS ────────────────────────────────────────

  void Bridge::emitToLua(const std::string& name, const nlohmann::json& payload) {
    if (!_MainLuaState.lua_state()) {
      debug::warn("Bridge::emitToLua: Lua state is not available");
      return;
    }

    sol::protected_function dispatchFn = _MainLuaState["coconut"]["_dispatch"];
    if (!dispatchFn.valid()) {
      debug::warn("Bridge::emitToLua: coconut._dispatch not found");
      return;
    }

    // Convert JSON payload to Lua table via common::toTable
    sol::table payloadTable = common::toTable(_MainLuaState, payload);

    auto result = dispatchFn(name, payloadTable, "");
    if (!result.valid()) {
      sol::error err = result;
      debug::warn(
          std::format("Bridge::emitToLua('{}'): coconut._dispatch failed: {}", name, err.what())
      );
    }
  }

  void Bridge::emitToJS(const std::string& eventName, const nlohmann::json& payload) {
    if (!_WebViewHandle) {
      debug::warn("Bridge::emitToJS: webview handle is not available");
      return;
    }

    // Build the JS call: globalThis.__coconut_emit(eventName, jsonPayload)
    std::string payloadStr;
    try {
      payloadStr = payload.dump();
    } catch (const std::exception& e) {
      debug::warn(std::format("Bridge::emitToJS: JSON serialization failed: {}", e.what()));
      payloadStr = "{}";
    }

    const std::string script = std::format(
        "globalThis.__coconut_emit('{}', {});", common::escapeString(eventName, '\''), payloadStr
    );

    webview_eval(_WebViewHandle, script.c_str());
  }

  void Bridge::callLuaCommand(const std::string& name, const nlohmann::json& args) {
    if (!_WorkerPool) {
      debug::warn("Bridge::callLuaCommand: worker pool not configured");
      return;
    }

    auto err = _WorkerPool->queueMessage(name, args);
    if (err) {
      debug::warn(std::format("Bridge::callLuaCommand('{}'): queue failed: {}", name, err->message)
      );
    }
  }

}  // namespace coconut::core
