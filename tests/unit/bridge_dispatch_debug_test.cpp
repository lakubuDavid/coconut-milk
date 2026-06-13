#include "bridge.h"
#include "config.h"
#include "debug.h"
#include "rpc_envelope.h"
#include "test.h"

#include <string>

// ── RPC envelope tests ────────────────────────────────────────────────

COCONUT_TEST(unit, rpc_message_default) {
  coconut::rpc::Message msg{};
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kEvent);
  COCONUT_REQUIRE(msg.id.empty());
  COCONUT_REQUIRE(msg.name.empty());
  COCONUT_REQUIRE(msg.payload.is_null());
}

COCONUT_TEST(unit, rpc_message_with_values) {
  coconut::rpc::Message msg{
    .type = coconut::rpc::Type::kReturn,
    .id = "42",
    .name = "ping",
    .payload = {{"key", "value"}}
  };
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kReturn);
  COCONUT_REQUIRE_EQ(msg.id, std::string("42"));
  COCONUT_REQUIRE_EQ(msg.name, std::string("ping"));
  COCONUT_REQUIRE_EQ(msg.payload["key"], std::string("value"));
}

COCONUT_TEST(unit, rpc_type_values) {
  auto call  = coconut::rpc::Type::kCall;
  auto ret   = coconut::rpc::Type::kReturn;
  auto err   = coconut::rpc::Type::kError;
  auto evt   = coconut::rpc::Type::kEvent;
  auto ready = coconut::rpc::Type::kReady;

  COCONUT_REQUIRE(call  != ret);
  COCONUT_REQUIRE(ret   != err);
  COCONUT_REQUIRE(err   != evt);
  COCONUT_REQUIRE(evt   != ready);
}

// ── RPC type serialization ────────────────────────────────────────────

COCONUT_TEST(unit, rpc_type_to_string) {
  COCONUT_REQUIRE_EQ(coconut::rpc::typeToString(coconut::rpc::Type::kCall),   std::string("call"));
  COCONUT_REQUIRE_EQ(coconut::rpc::typeToString(coconut::rpc::Type::kReturn), std::string("return"));
  COCONUT_REQUIRE_EQ(coconut::rpc::typeToString(coconut::rpc::Type::kError),  std::string("error"));
  COCONUT_REQUIRE_EQ(coconut::rpc::typeToString(coconut::rpc::Type::kEvent),  std::string("event"));
  COCONUT_REQUIRE_EQ(coconut::rpc::typeToString(coconut::rpc::Type::kReady),  std::string("ready"));
}

COCONUT_TEST(unit, rpc_type_from_string) {
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("call"),   coconut::rpc::Type::kCall);
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("return"), coconut::rpc::Type::kReturn);
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("error"),  coconut::rpc::Type::kError);
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("event"),  coconut::rpc::Type::kEvent);
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("ready"),  coconut::rpc::Type::kReady);
}

COCONUT_TEST(unit, rpc_type_from_unknown_returns_event) {
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString("unknown"), coconut::rpc::Type::kEvent);
}

COCONUT_TEST(unit, rpc_type_from_empty_returns_event) {
  COCONUT_REQUIRE_EQ(coconut::rpc::typeFromString(""), coconut::rpc::Type::kEvent);
}

// ── RPC message serialization ─────────────────────────────────────────

COCONUT_TEST(unit, rpc_message_to_json) {
  coconut::rpc::Message msg{
    .type = coconut::rpc::Type::kCall,
    .id = "1",
    .name = "ping",
    .payload = {{"arg", 42}}
  };
  nlohmann::json j = msg.toJson();
  COCONUT_REQUIRE_EQ(j["type"], std::string("call"));
  COCONUT_REQUIRE_EQ(j["id"], std::string("1"));
  COCONUT_REQUIRE_EQ(j["name"], std::string("ping"));
  COCONUT_REQUIRE_EQ(j["payload"]["arg"], 42);
}

COCONUT_TEST(unit, rpc_message_from_json) {
  nlohmann::json j = {
    {"type", "return"},
    {"id", "42"},
    {"name", "getConfig"},
    {"payload", {{"result", "ok"}}}
  };
  auto msg = coconut::rpc::Message::fromJson(j);
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kReturn);
  COCONUT_REQUIRE_EQ(msg.id, std::string("42"));
  COCONUT_REQUIRE_EQ(msg.name, std::string("getConfig"));
  COCONUT_REQUIRE_EQ(msg.payload["result"], std::string("ok"));
}

COCONUT_TEST(unit, rpc_message_roundtrip) {
  coconut::rpc::Message original{
    .type = coconut::rpc::Type::kEvent,
    .name = "update",
    .payload = {{"count", 5}}
  };
  nlohmann::json j = original.toJson();
  auto restored = coconut::rpc::Message::fromJson(j);
  COCONUT_REQUIRE_EQ(restored.type, original.type);
  COCONUT_REQUIRE_EQ(restored.name, original.name);
  COCONUT_REQUIRE_EQ(restored.payload["count"], 5);
}

COCONUT_TEST(unit, rpc_message_from_json_null) {
  nlohmann::json j;
  auto msg = coconut::rpc::Message::fromJson(j);
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kEvent);
  COCONUT_REQUIRE(msg.id.empty());
}

COCONUT_TEST(unit, rpc_message_from_json_empty_string) {
  auto msg = coconut::rpc::Message::fromJson(std::string(""));
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kEvent);
}

COCONUT_TEST(unit, rpc_message_from_json_invalid) {
  auto msg = coconut::rpc::Message::fromJson(std::string("not valid json"));
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kEvent);
}

COCONUT_TEST(unit, rpc_message_from_json_partial) {
  nlohmann::json j = {{"type", "error"}};
  auto msg = coconut::rpc::Message::fromJson(j);
  COCONUT_REQUIRE_EQ(msg.type, coconut::rpc::Type::kError);
  COCONUT_REQUIRE(msg.id.empty());
  COCONUT_REQUIRE(msg.name.empty());
  COCONUT_REQUIRE(msg.payload.is_null());
}

// ── Bridge dispatch (null-app safety) ─────────────────────────────────

COCONUT_TEST(unit, bridge_emit_to_lua_null_app) {
  coconut::bridge::emitToLua(nullptr, "test", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_emit_to_js_null_app) {
  coconut::bridge::emitToJS(nullptr, "test", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_call_lua_null_app) {
  coconut::bridge::callLua(nullptr, "test", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_call_js_null_app) {
  coconut::bridge::callJS(nullptr, "test", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_emit_to_lua_empty_event) {
  coconut::bridge::emitToLua(nullptr, "", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_emit_to_js_empty_event) {
  coconut::bridge::emitToJS(nullptr, "", nlohmann::json::object());
}

COCONUT_TEST(unit, bridge_rpc_send_null_app) {
  coconut::bridge::rpcSend(nullptr, coconut::rpc::Message{});
}

COCONUT_TEST(unit, bridge_signal_ready_null_app) {
  coconut::bridge::signalReady(nullptr);
}

// ── Debug transport dump flag ─────────────────────────────────────────

COCONUT_TEST(unit, debug_transport_dump_default_false) {
  coconut::Config cfg{};
  COCONUT_REQUIRE(!cfg.debug.showTransportDump);
}

COCONUT_TEST(unit, debug_transport_dump_enabled) {
  coconut::Config cfg{};
  cfg.debug.showTransportDump = true;
  COCONUT_REQUIRE(cfg.debug.showTransportDump);
}

COCONUT_TEST(unit, debug_transport_dump_disabled) {
  coconut::Config cfg{};
  cfg.debug.showTransportDump = false;
  COCONUT_REQUIRE(!cfg.debug.showTransportDump);
}
