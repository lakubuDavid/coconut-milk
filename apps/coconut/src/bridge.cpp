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

  static void dispatchRpcEventToLua(coconut::App* app, const rpc::Message& msg) {
    dispatchEventToLua(app, msg.name, msg.payload);
  }

  /// Route an incoming kCall RPC message to the command registry.
  /// Sends a kReturn or kError response through the transport.
  static void dispatchRpcCallToLua(coconut::App* app, const rpc::Message& msg) {
    if (app == nullptr || app->commands == nullptr || app->lua_state == nullptr ||
        app->lua_state->lua_state == nullptr) {
      return;
    }

    sol::state_view lua(*app->lua_state->lua_state);

    core::CommandResult result = core::execCommand(
        lua, app->commands->handlers, msg.name, msg.payload, app->lua_state->context
    );

    rpc::Message reply;
    reply.id = msg.id;

    if (result.ok) {
      reply.type    = rpc::Type::kReturn;
      reply.payload = result.data;
    } else {
      reply.type = rpc::Type::kError;
      debug::warn(std::format(
          "bridge: cmd '{}' failed: {}", msg.name, result.data.value("message", "unknown error")
      ));
      reply.payload = result.data;
    }

    rpcSend(app, reply);
  }

  // ---------------------------------------------------------------------------
  // JS bridge helpers
  // ---------------------------------------------------------------------------

  void emitToJS(coconut::App* app, std::string eventName, nlohmann::json payload) {
    // Route through the transport as an RPC event.
    rpc::Message msg;
    msg.type    = rpc::Type::kEvent;
    msg.name    = std::move(eventName);
    msg.payload = std::move(payload);
    rpcSend(app, msg);
  }

  void callJS(coconut::App* app, std::string functionName, nlohmann::json payload) {
    if (app == nullptr || app->window == nullptr || app->webview == nullptr) {
      return;
    }

    std::string payloadStr;
    try {
      payloadStr = payload.dump();
    } catch (const std::exception&) {
      payloadStr = "{}";
    }
    const std::string script = std::format("globalThis['{}']({});", functionName, payloadStr);

    webview_eval(app->webview, script.c_str());
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
    coconut_js += R"(
// Shim: forward __coconut_call through the __coconut_rpc webview binding.
// webview_bind creates an async function; webview_return() resolves the promise.
globalThis.__coconut_call = async function(name, payloadJson) {
  var msg = JSON.stringify({ type: "call", name: name, payload: JSON.parse(payloadJson) });
  var result = await globalThis.__coconut_rpc(msg);
  // __coconut_rpc returns the parsed envelope object; re-stringify for coconut.call()
  return JSON.stringify(result);
};
globalThis.__coconut_emit = async function(name, payloadJson) {
  var msg = JSON.stringify({ type: "event", name: name, payload: JSON.parse(payloadJson) });
  await globalThis.__coconut_rpc(msg);
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
    auto* t                      = new WebviewTransport(app->webview, app, coconut_js);
    app->bridge_state->transport = t;
  }

  /// Signal to the frontend that the bridge is ready.
  /// With webview this is a no-op — kReady is baked into the init script
  /// passed to webview_init() in createTransport().
  void signalReady(coconut::App* app) {
    (void)app;
    // kReady fires automatically via webview_init script.
  }

  void rpcSend(coconut::App* app, const rpc::Message& msg) {
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

    delete state->transport;  // WebviewTransport
    state->transport = nullptr;
    delete state;
  }

}  // namespace coconut::bridge
