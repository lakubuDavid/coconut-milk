#include "bridge.h"
#include "common.h"
#include "debug.h"
#include "rpc_envelope.h"  // rpc::Message, rpc::Type

#include <format>
#include <string>

namespace coconut::core {

  // ── BridgeBuilder ───────────────────────────────────────────────────

  BridgeBuilder& BridgeBuilder::withTransport(std::shared_ptr<transport::Transport> transport) {
    TransportPtr = std::move(transport);
    return *this;
  }

  BridgeBuilder& BridgeBuilder::withLuaState(sol::state_view lua) {
    LuaState = lua;
    return *this;
  }

  std::expected<std::unique_ptr<Bridge>, coconut::Error> BridgeBuilder::build() {
    if (!TransportPtr) {
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "BridgeBuilder::build: transport is null"});
    }

    if (!LuaState.lua_state()) {
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "BridgeBuilder::build: Lua state is null"});
    }

    return std::unique_ptr<Bridge>(new Bridge(std::move(TransportPtr), LuaState));
  }

  // ── Bridge ──────────────────────────────────────────────────────────

  BridgeBuilder Bridge::builder() {
    return BridgeBuilder{};
  }

  Bridge::Bridge(std::shared_ptr<transport::Transport> transport, sol::state_view lua)
      : _Transport(std::move(transport)), _MainLuaState(lua) {
  }

  void Bridge::setCommandCallHandler(std::function<void(CommandCallMessage)> handler) {
    _CommandCallHandler = std::move(handler);
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

    auto result = dispatchFn(name, payloadTable, "");
    if (!result.valid()) {
      sol::error err = result;
      debug::warn(
          std::format("Bridge::emitToLua('{}'): coconut._dispatch failed: {}", name, err.what())
      );
    }
  }

  // ── Outbound: C++ → JS ──────────────────────────────────────────────

  void Bridge::rpcSend(const rpc::Message& msg) {
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
    rpc::Message rpcMsg;
    rpcMsg.type    = rpc::Type::kEvent;
    rpcMsg.name    = eventName;
    rpcMsg.payload = payload;
    rpcSend(rpcMsg);
  }

}  // namespace coconut::core
