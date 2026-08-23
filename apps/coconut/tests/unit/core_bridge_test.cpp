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
    std::vector<std::string>                 evaluated;
    std::vector<coconut::core::JsRPCMessage> sent;

    void send(const coconut::core::JsRPCMessage& msg) override {
      sent.push_back(msg);
    }
    void eval(const std::string& js) override {
      evaluated.push_back(js);
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

// ── onInbound routing (Phase 2 inbound cutover) ─────────────────────────────

COCONUT_TEST(core_bridge, oninbound_event_goes_to_lua_dispatch) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  int         events = 0;
  std::string seenName;
  lua["coconut"] = lua.create_table_with(
      "_dispatch",
      [&](std::string name, sol::table /*payload*/, std::string /*target*/) {
        ++events;
        seenName = name;
      }
  );

  FakeTransport transport;
  auto          bridge = makeBridge(transport, lua);

  core::JsRPCMessage msg;
  msg.type    = core::RpcType::kEvent;
  msg.name    = "user-login";
  msg.payload = {{"id", 42}};
  bridge->onInbound(msg);

  COCONUT_REQUIRE_EQ(events, 1);
  COCONUT_REQUIRE_EQ(seenName, std::string("user-login"));
  COCONUT_REQUIRE(transport.sent.empty());  // events need no reply
}

COCONUT_TEST(core_bridge, oninbound_call_sync_executor_replies_envelope) {
  sol::state    lua;
  FakeTransport transport;
  auto          result = core::Bridge::builder()
                    .withTransport(std::shared_ptr<coconut::transport::Transport>(
                        &transport, [](coconut::transport::Transport*) {}
                    ))
                    .withLuaState(sol::state_view(lua))
                    .withSyncExecutor(
                        [](const std::string& name,
                           const nlohmann::json&) -> std::optional<core::CommandResult> {
                          if (name == "dialog_open") {
                            return core::CommandResult{.ok = true, .data = {{"picked", "a.txt"}}};
                          }
                          return std::nullopt;  // fall through to workers
                        }
                    )
                    .build();
  COCONUT_REQUIRE(result.has_value());
  auto bridge = std::move(result.value());

  // Sync-owned command: replied immediately via envelope.
  core::JsRPCMessage call;
  call.type = core::RpcType::kCall;
  call.id   = "rpc-1";
  call.name = "dialog_open";
  bridge->onInbound(call);

  COCONUT_REQUIRE_EQ(transport.sent.size(), size_t{1});
  COCONUT_REQUIRE_EQ(transport.sent[0].type, core::RpcType::kReturn);
  COCONUT_REQUIRE_EQ(transport.sent[0].id, std::string("rpc-1"));
  COCONUT_REQUIRE_EQ(transport.sent[0].payload["ok"], true);
  COCONUT_REQUIRE_EQ(transport.sent[0].payload["data"]["picked"], std::string("a.txt"));

  // Sync error path.
  core::JsRPCMessage bad;
  bad.type = core::RpcType::kCall;
  bad.id   = "rpc-2";
  bad.name = "__never__";
  transport.sent.clear();
  bridge.reset();
  auto result2 =
      core::Bridge::builder()
          .withTransport(std::shared_ptr<coconut::transport::Transport>(
              &transport, [](coconut::transport::Transport*) {}
          ))
          .withLuaState(sol::state_view(lua))
          .withSyncExecutor(
              [](const std::string&, const nlohmann::json&) -> std::optional<core::CommandResult> {
                return core::CommandResult{
                    .ok = false, .data = {{"code", "LuaError"}, {"message", "boom"}}};
              }
          )
          .build();
  auto bridge2 = std::move(result2.value());
  bridge2->onInbound(bad);
  COCONUT_REQUIRE_EQ(transport.sent.size(), size_t{1});
  COCONUT_REQUIRE_EQ(transport.sent[0].type, core::RpcType::kError);
  COCONUT_REQUIRE_EQ(transport.sent[0].payload["ok"], false);
}

COCONUT_TEST(core_bridge, oninbound_unknown_call_falls_through_to_worker_handler) {
  sol::state    lua;
  FakeTransport transport;
  auto          result =
      core::Bridge::builder()
          .withTransport(std::shared_ptr<coconut::transport::Transport>(
              &transport, [](coconut::transport::Transport*) {}
          ))
          .withLuaState(sol::state_view(lua))
          .withSyncExecutor(
              [](const std::string&, const nlohmann::json&) -> std::optional<core::CommandResult> {
                return std::nullopt;
              }
          )
          .build();
  auto bridge = std::move(result.value());

  int            forwarded = 0;
  std::string    seenName;
  std::string    seenRpcId;
  nlohmann::json seenArgs;
  bridge->setCommandCallHandler([&](core::CommandCallMessage m) {
    ++forwarded;
    seenName  = m.CommandName;
    seenRpcId = m.RpcId;
    seenArgs  = m.Args;
  });

  core::JsRPCMessage call;
  call.type    = core::RpcType::kCall;
  call.id      = "call-xyz";
  call.name    = "resize-image";
  call.payload = {{"w", 100}};
  bridge->onInbound(call);

  COCONUT_REQUIRE_EQ(forwarded, 1);
  COCONUT_REQUIRE_EQ(seenName, std::string("resize-image"));
  COCONUT_REQUIRE_EQ(seenRpcId, std::string("call-xyz"));  // correlation preserved
  COCONUT_REQUIRE_EQ(seenArgs["w"], 100);
  COCONUT_REQUIRE(transport.sent.empty());  // async — no immediate reply
}

COCONUT_TEST(core_bridge, oninbound_unroutable_call_replies_error_envelope) {
  sol::state    lua;
  FakeTransport transport;
  auto          result = core::Bridge::builder()
                    .withTransport(std::shared_ptr<coconut::transport::Transport>(
                        &transport, [](coconut::transport::Transport*) {}
                    ))
                    .withLuaState(sol::state_view(lua))
                    .build();  // no sync executor, no command handler
  auto bridge = std::move(result.value());

  core::JsRPCMessage call;
  call.type = core::RpcType::kCall;
  call.id   = "rpc-9";
  call.name = "ghost";
  bridge->onInbound(call);

  COCONUT_REQUIRE_EQ(transport.sent.size(), size_t{1});
  COCONUT_REQUIRE_EQ(transport.sent[0].type, core::RpcType::kError);
  COCONUT_REQUIRE_EQ(transport.sent[0].id, std::string("rpc-9"));
  COCONUT_REQUIRE_EQ(transport.sent[0].payload["error"]["code"], std::string("NoRoute"));
}
