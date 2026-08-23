#include "bridge.h"
#include "common.h"
#include "debug.h"

#include <format>
#include <string>

namespace coconut::core {

  // ── BridgeBuilder ───────────────────────────────────────────────────

  BridgeBuilder& BridgeBuilder::withTransport(std::shared_ptr<transport::Transport> transport) {
    TransportPtr = std::move(transport);
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withLuaState(sol::state_view lua) {
    LuaState.emplace(lua);
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withSyncExecutor(
      std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)> fn
  ) {
    SyncExecutor = std::move(fn);
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withTargetResolver(std::function<std::string()> fn) {
    TargetResolver = std::move(fn);
    return *this;
  }

  std::expected<std::unique_ptr<Bridge>, coconut::Error> BridgeBuilder::build() {
    if (!TransportPtr) {
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "BridgeBuilder::build: transport is null"});
    }

    if (!LuaState.has_value() || !LuaState->lua_state()) {
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "BridgeBuilder::build: Lua state is null"});
    }

    return std::unique_ptr<Bridge>(new Bridge(
        std::move(TransportPtr), *LuaState, std::move(SyncExecutor), std::move(TargetResolver)
    ));
  }

  // ── Bridge ──────────────────────────────────────────────────────────

  BridgeBuilder Bridge::builder() {
    return BridgeBuilder{};
  }

  Bridge::Bridge(
      std::shared_ptr<transport::Transport> transport,
      sol::state_view                       lua,
      std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)>
                                   syncExecutor,
      std::function<std::string()> targetResolver
  )
      : _Transport(std::move(transport)),
        _MainLuaState(lua),
        _SyncExecutor(std::move(syncExecutor)),
        _TargetResolver(std::move(targetResolver)) {
  }

  void Bridge::setCommandCallHandler(std::function<void(CommandCallMessage)> handler) {
    _CommandCallHandler = std::move(handler);
  }

  // ── Inbound routing ─────────────────────────────────────────────────

  void Bridge::onInbound(const JsRPCMessage& msg) {
    switch (msg.type) {
      case RpcType::kEvent:
        emitToLua(msg.name, msg.payload);
        break;

      case RpcType::kCall: {
        // Main-thread-only commands answer synchronously on the spot.
        if (_SyncExecutor) {
          auto result = _SyncExecutor(msg.name, msg.payload);
          if (result.has_value()) {
            JsRPCMessage reply;
            reply.id      = msg.id;
            reply.type    = result->ok ? RpcType::kReturn : RpcType::kError;
            reply.payload = result->ok ? nlohmann::json{{"ok", true}, {"data", result->data}}
                                       : nlohmann::json{{"ok", false}, {"error", result->data}};
            rpcSend(reply);
            return;
          }
        }

        // Everything else routes to the workers (async envelope protocol).
        if (!_CommandCallHandler) {
          debug::warn("Bridge::onInbound: no command route for '" + msg.name + "'");
          JsRPCMessage reply;
          reply.id      = msg.id;
          reply.type    = RpcType::kError;
          reply.payload = {
              {"ok", false}, {"error", {{"code", "NoRoute"}, {"message", "no command route"}}}};
          rpcSend(reply);
          return;
        }
        _CommandCallHandler(CommandCallMessage{
            .CommandName = msg.name,
            .Args        = msg.payload,
            .RpcId       = msg.id,
        });
        break;
      }

      default:
        debug::warn("Bridge::onInbound: unexpected inbound RPC type for id='" + msg.id + "'");
        break;
    }
  }

  void Bridge::forwardCommandCall(const CommandCallMessage& msg) {
    if (_CommandCallHandler) {
      _CommandCallHandler(msg);
    }
  }

  // ── Inbound: JS → Lua ───────────────────────────────────────────────

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

    const std::string target = _TargetResolver ? _TargetResolver() : "";
    auto              result = dispatchFn(name, payloadTable, target);
    if (!result.valid()) {
      sol::error err = result;
      debug::warn(
          std::format("Bridge::emitToLua('{}'): coconut._dispatch failed: {}", name, err.what())
      );
    }
  }

  // ── Outbound: C++ → JS ──────────────────────────────────────────────

  void Bridge::rpcSend(const JsRPCMessage& msg) {
    if (_Transport) {
      _Transport->send(msg);
    }
  }

  void Bridge::emitToJS(const std::string& eventName, const nlohmann::json& payload) {
    if (!_Transport) {
      debug::warn("Bridge::emitToJS: transport is not available");
      return;
    }

    // Forward as an RPC event envelope; the transport maps kEvent →
    // globalThis.__coconut_dispatch_event(name, payloadJson).
    JsRPCMessage rpcMsg;
    rpcMsg.type    = RpcType::kEvent;
    rpcMsg.name    = eventName;
    rpcMsg.payload = payload;
    rpcSend(rpcMsg);
  }

}  // namespace coconut::core
