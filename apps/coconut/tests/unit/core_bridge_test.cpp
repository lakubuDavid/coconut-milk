/// Unit tests for core::Bridge (builder validation + routing).
///
/// Uses a FakeTransport to capture outbound JsRPCMessage sends without
/// a real webview, verifying:
///   - Builder validation (null transport / null Lua state rejected)
///   - emitToJS wraps the event as a kEvent JsRPCMessage envelope via the transport
///   - rpcSend forwards raw envelopes
///   - forwardCommandCall invokes the injected handler (and is a safe
///     no-op without one) — the Bridge holds no WorkerPool dependency

#include "core/bridge.h"

#include "rpc_envelope.h"
#include "test.h"
#include "transport.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

  /// Captures every send() for inspection. Never owns a webview.
  struct FakeTransport : public coconut::transport::Transport {
    std::vector<coconut::core::JsRPCMessage> sent;

    void send(const coconut::core::JsRPCMessage& msg) override {
      sent.push_back(msg);
    }
    void setMessageCallback(coconut::transport::MessageCallback cb) override {
      callback = std::move(cb);
    }

    coconut::transport::MessageCallback callback;
  };

  /// Build a Bridge over a fake transport with a real (empty) Lua state.
  std::unique_ptr<coconut::core::Bridge> makeBridge(FakeTransport& transport, sol::state& lua) {
    auto result = coconut::core::Bridge::builder()
                      .withTransport(std::shared_ptr<coconut::transport::Transport>(
                          &transport, [](coconut::transport::Transport*) {}
                      ))
                      .withLuaState(sol::state_view(lua))
                      .build();
    if (!result) {
      throw std::runtime_error("makeBridge: " + result.error().message);
    }
    return std::move(result.value());
  }

}  // namespace

using namespace coconut;

COCONUT_TEST(core_bridge, builder_rejects_null_transport) {
  sol::state lua;
  auto       result = core::Bridge::builder().withLuaState(sol::state_view(lua)).build();
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, ErrorCode::InvalidConfig);
}

COCONUT_TEST(core_bridge, builder_rejects_null_lua_state) {
  FakeTransport transport;
  auto          result = core::Bridge::builder()
                    .withTransport(std::shared_ptr<transport::Transport>(
                        &transport, [](transport::Transport*) {}
                    ))
                    .build();
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, ErrorCode::InvalidConfig);
}

COCONUT_TEST(core_bridge, builder_accepts_valid_config) {
  sol::state    lua;
  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);
  COCONUT_REQUIRE(bridge != nullptr);
}

COCONUT_TEST(core_bridge, emit_to_js_sends_kEvent_envelope) {
  sol::state    lua;
  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);

  bridge->emitToJS("workspace-updated", nlohmann::json{{"id", 7}});

  COCONUT_REQUIRE_EQ(transport.sent.size(), size_t{1});
  const auto& msg = transport.sent[0];
  COCONUT_REQUIRE(msg.type == coconut::core::RpcType::kEvent);
  COCONUT_REQUIRE_EQ(msg.name, std::string("workspace-updated"));
  COCONUT_REQUIRE(msg.payload.value("id", 0) == 7);
}

COCONUT_TEST(core_bridge, rpc_send_forwards_raw_envelope) {
  sol::state    lua;
  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);

  coconut::core::JsRPCMessage msg;
  msg.id      = "call-42";
  msg.type    = coconut::core::RpcType::kReturn;
  msg.payload = nlohmann::json{{"ok", true}};
  bridge->rpcSend(msg);

  COCONUT_REQUIRE_EQ(transport.sent.size(), size_t{1});
  COCONUT_REQUIRE(transport.sent[0].type == coconut::core::RpcType::kReturn);
  COCONUT_REQUIRE_EQ(transport.sent[0].id, std::string("call-42"));
}

COCONUT_TEST(core_bridge, forward_command_call_invokes_handler) {
  sol::state    lua;
  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);

  int         calls = 0;
  std::string seenName;
  bridge->setCommandCallHandler([&](core::CommandCallMessage m) {
    ++calls;
    seenName = m.CommandName;
  });

  bridge->forwardCommandCall(core::CommandCallMessage{.CommandName = "ping", .Args = nullptr});
  COCONUT_REQUIRE_EQ(calls, 1);
  COCONUT_REQUIRE_EQ(seenName, std::string("ping"));
}

COCONUT_TEST(core_bridge, forward_command_call_noop_without_handler) {
  sol::state    lua;
  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);

  // Must not crash when no handler is registered.
  bridge->forwardCommandCall(core::CommandCallMessage{.CommandName = "ping", .Args = nullptr});
  COCONUT_REQUIRE(transport.sent.empty());
}
