#include "webview_transport.h"
#include "test.h"

#include <string>

// ── WebviewTransport requires a real webview_t ────────────────────────
// The transport class is deeply coupled to the native webview handle
// (WKWebView / WebView2). Constructing one without a valid handle is
// a no-op. The core RPC message types and JSON serialization are tested
// separately in bridge_roundtrip_test.cpp.
//
// These tests verify that the header is self-consistent and that the
// static callback signatures compile correctly.

COCONUT_TEST(unit, webview_transport_header_includes) {
  // Verify the transport namespace and types are accessible
  using coconut::bridge::WebviewTransport;
  using coconut::transport::MessageCallback;
  using coconut::transport::Transport;

  // The types exist and are meaningful
  bool is_transport = std::is_base_of<Transport, WebviewTransport>::value;
  COCONUT_REQUIRE(is_transport);
}

COCONUT_TEST(unit, webview_transport_null_handle_noop) {
  // Create transport with null handle — should not crash
  coconut::bridge::WebviewTransport transport(nullptr, nullptr, "");

  COCONUT_REQUIRE(transport.handle() == nullptr);
}

COCONUT_TEST(unit, webview_transport_set_message_callback) {
  coconut::bridge::WebviewTransport transport(nullptr, nullptr, "");

  // Setting a null callback should not crash
  transport.setMessageCallback(nullptr);

  // Setting a valid callback should also not crash
  bool called = false;
  transport.setMessageCallback([&called](const coconut::core::JsRPCMessage&) { called = true; });
  // (message callback won't fire without a real webview)
}

// ── RPC envelope types (compile-time check) ──────────────────────────

COCONUT_TEST(unit, webview_transport_rpc_types) {
  // Verify that core::JsRPCMessage and related enums are accessible
  // (defined in core/messages.h, included by transport.h)

  coconut::core::JsRPCMessage msg;
  msg.id      = "test-1";
  msg.type    = coconut::core::RpcType::kCall;
  msg.name    = "my_command";
  msg.payload = nullptr;

  COCONUT_REQUIRE_EQ(msg.id, std::string("test-1"));
  COCONUT_REQUIRE_EQ(static_cast<int>(msg.type), static_cast<int>(coconut::core::RpcType::kCall));
  COCONUT_REQUIRE_EQ(msg.name, std::string("my_command"));
}

// Note: Full transport integration tests require a live webview handle
// and are covered by the e2e test suite (tests/integration/*.cpp).
