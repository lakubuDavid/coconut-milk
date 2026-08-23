#include "bridge.h"
#include "app.h"
#include "common.h"
#include "core/exec_command.h"
#include "debug.h"
#include "embeds/coconut_embed.h"
#include "main_runtime.h"
#include "modules/json.h"
#include "webview_transport.h"

#include <format>
#include <iostream>
#include <memory>
#include <string>

namespace coconut::bridge {

  std::expected<State*, Error> create(Config* config) {
    if (!config) {
      return std::unexpected(Error{
          .code = ErrorCode::InvalidConfig, .message = "bridge::create: config is null"});
    }
    return new State{.configs = config};
  }

  // ---------------------------------------------------------------------------
  // Inbound RPC dispatch helpers (bridge owns the dispatch logic)
  // ---------------------------------------------------------------------------

  /// Route an incoming kEvent RPC message through the three-tier dispatch chain.
  /// Calls coconut._dispatch(name, payload, target).
  void dispatchEventToLua(
      coconut::App* app, const std::string& name, const nlohmann::json& payload
  ) {
    if (app == nullptr || app->lua_state == nullptr || app->lua_state->lua_state == nullptr ||
        app->lua_state->context == nullptr) {
      return;
    }

    sol::state_view lua(*app->lua_state->lua_state);

    // Build a Lua table from the JSON payload
    sol::table payloadTable = common::toTable(lua, payload);

    // Determine the active view for target
    std::string target = app->window ? app->window->current_view : "";

    // Route through the central dispatch: coconut._dispatch(name, payload, target)
    sol::function dispatch = lua["coconut"]["_dispatch"];
    if (!dispatch.valid()) {
      debug::warn("dispatchEventToLua: coconut._dispatch not found");
      // Fallback: try old-style coconut.events(event) for backward compat
      sol::function fallback = lua["coconut"]["events"];
      if (fallback.valid()) {
        fallback(payloadTable);
      }
      return;
    }

    auto result = dispatch(name, payloadTable, target);
    if (!result.valid()) {
      sol::error err = result;
      debug::warn(
          std::format("dispatchEventToLua('{}'): coconut._dispatch failed: {}", name, err.what())
      );
    }
  }

  // ---------------------------------------------------------------------------
  // JS bridge helpers
  // ---------------------------------------------------------------------------

  void emitToJS(coconut::App* app, std::string eventName, nlohmann::json payload) {
    // Route through the transport as an RPC event.
    coconut::core::JsRPCMessage msg;
    msg.type    = coconut::core::RpcType::kEvent;
    msg.name    = std::move(eventName);
    msg.payload = std::move(payload);
    rpcSend(app, msg);
  }

  // ---------------------------------------------------------------------------
  // Webview transport — wraps webview_bind / webview_eval behind Transport interface
  // ---------------------------------------------------------------------------

  static std::string decodeEmbed() {
    size_t len = sizeof(coconut_js_embed);
    if (len > 0 && coconut_js_embed[len - 1] == 0)
      len -= 1;
    return std::string(reinterpret_cast<const char*>(coconut_js_embed), len);
  }

  void createTransport(coconut::App* app) {
    if (app == nullptr || app->webview == nullptr || app->bridge_state == nullptr) {
      return;
    }

    // Decode the embedded Coconut JS runtime.
    std::string coconut_js = decodeEmbed();

    // Append shims for old __coconut_call / __coconut_emit names that
    // coconut.ts still references, and auto-fire the bridge-ready signal.
    //
    // Reply protocol (uniformly async):
    //   __coconut_call generates a unique id, sends it inside the envelope,
    //   and parks a promise resolver keyed by that id. The C++ side replies
    //   with a kReturn/kError envelope carrying the same id, delivered via
    //   __coconut_rpc_receive — webview_return is never used for commands.
    coconut_js += R"(
// Promise correlation: id → {resolve, reject}
globalThis.__coconut_pendingRpc = new Map();
var __coconut_rpcSeq = 0;

// Inbound reply envelopes from C++ ({id, type: "return"|"error", payload}).
// payload is the {ok:true,data} or {ok:false,error} envelope.
globalThis.__coconut_rpc_receive = function(msgJson) {
  var m;
  try { m = JSON.parse(msgJson); } catch (e) { return; }
  if (!m || !m.id) return;
  var pending = globalThis.__coconut_pendingRpc.get(m.id);
  if (!pending) return;
  globalThis.__coconut_pendingRpc.delete(m.id);
  if (m.type === 'error') {
    pending.reject(m.payload && m.payload.error ? m.payload.error : m.payload);
  } else {
    // coconut.call() expects __coconut_call to return a JSON STRING of the
    // {ok,data}/{ok,error} envelope (it JSON.parses it) — stringify here.
    pending.resolve(JSON.stringify(m.payload));
  }
};

// Shim: forward __coconut_call through the __coconut_rpc webview binding.
globalThis.__coconut_call = function(name, payloadJson) {
  return new Promise(function(resolve, reject) {
    var id = 'rpc-' + (++__coconut_rpcSeq) + '-' + Date.now();
    var msg = JSON.stringify({ type: 'call', id: id, name: name, payload: JSON.parse(payloadJson) });
    globalThis.__coconut_pendingRpc.set(id, { resolve: resolve, reject: reject });
    try {
      var p = globalThis.__coconut_rpc(msg);
      // Safety net: if the binding itself rejects (bridge down), fail fast.
      if (p && typeof p.catch === 'function') {
        p.catch(function(e) {
          if (globalThis.__coconut_pendingRpc.delete(id)) {
            reject({ code: 'E_BRIDGE_DOWN', message: String(e) });
          }
        });
      }
    } catch (e) {
      globalThis.__coconut_pendingRpc.delete(id);
      reject({ code: 'E_BRIDGE_DOWN', message: String(e) });
    }
  });
};

// Events are fire-and-forget: send and return immediately. The webview_bind
// promise is intentionally left unresolved (no webview_return for events).
globalThis.__coconut_emit = function(name, payloadJson) {
  var msg = JSON.stringify({ type: 'event', name: name, payload: JSON.parse(payloadJson) });
  try {
    var p = globalThis.__coconut_rpc(msg);
    if (p && typeof p.catch === 'function') p.catch(function() {});
  } catch (e) { /* ignore bridge errors */ }
  return Promise.resolve('');
};

// Debug logging bridged to C++ via __coconut_rpc events.
// These are the JS equivalents of coconut.log/.info/.warn/.error on the Lua side.
globalThis.coconut.log   = function(...args) { console.log('[coconut]', ...args); };
globalThis.coconut.info  = function(...args) { console.info('[coconut]', ...args); };
globalThis.coconut.warn  = function(...args) { console.warn('[coconut]', ...args); };
globalThis.coconut.error = function(...args) { console.error('[coconut]', ...args); };

// Top-level error handler — logs uncaught JS exceptions to stderr.
globalThis.addEventListener('error', function(e) {
  console.error('[coconut:uncaught]', e.message, 'at', e.filename + ':' + e.lineno);
});
globalThis.addEventListener('unhandledrejection', function(e) {
  console.error('[coconut:unhandled]', e.reason);
});

// Patch console to bridge log messages to C++ stderr via __coconut_emit.
(function() {
  var levels = {log: 'log', warn: 'warn', error: 'error', info: 'info'};
  for (var name in levels) {
    var orig = console[name];
    console[name] = function() {
      var args = Array.prototype.slice.call(arguments);
      orig.apply(console, args);
      try {
        var payload = args.map(function(a) { return typeof a === 'string' ? a : JSON.stringify(a); }).join(' ');
        __coconut_emit('__console__' + name, JSON.stringify({message: payload}));
      } catch(e) { /* ignore bridge errors */ }
    };
  }
})();

globalThis.__coconut_bridge_ready();
)";

    // Create the webview transport, which calls:
    //   webview_init()  — injects Coconut JS runtime (fires on next page load)
    //   webview_bind()  — registers __coconut_rpc for inbound messages
    // The transport handles dispatch internally (via App* reference).
    // Held as shared_ptr so the core Bridge/Dispatcher can co-own it.
    app->bridge_state->transport =
        std::make_shared<WebviewTransport>(app->webview, app, coconut_js);
  }

  void rpcSend(coconut::App* app, const coconut::core::JsRPCMessage& msg) {
    if (app == nullptr || app->bridge_state == nullptr || app->bridge_state->transport == nullptr) {
      return;
    }
    app->bridge_state->transport->send(msg);
  }

  void destroy(State* state) {
    if (state == nullptr) {
      return;
    }

    // Destroy the store
    if (state->store) {
      store::destroy(state->store);
      state->store = nullptr;
    }

    // Transport is shared_ptr — released when the last owner drops it.
    delete state;
  }

}  // namespace coconut::bridge
