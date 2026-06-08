#include "webview_transport.h"
#include "bridge.h" // for escapeJsSingleQuotedString, toJson

#include <nlohmann/json.hpp>

#include <format>
#include <iostream>
#include <string>

namespace coconut::bridge {

WebviewTransport::WebviewTransport(webview_t w, const std::string& coconut_js)
    : m_webview(w) {
  if (!w) {
    std::cerr << "[webview] WebviewTransport: null handle\n";
    return;
  }

  // Inject the Coconut JS runtime before any page loads.
  if (!coconut_js.empty()) {
    webview_init(w, coconut_js.c_str());
  }

  // Bind the inbound RPC channel.
  // JS calls: window.__coconut_rpc(msgJson)
  // webview delivers: req = JSON.stringify([msgJson])
  webview_bind(w, "__coconut_rpc", &WebviewTransport::static_on_rpc, this);

  m_inited = true;
}

WebviewTransport::~WebviewTransport() {
  // webview_destroy is owned by the window/app, not by the transport.
  m_webview = nullptr;
}

void WebviewTransport::send(const rpc::Message& msg) {
  if (!m_webview) return;

  switch (msg.type) {
    case rpc::Type::kEvent: {
      // Keep the existing __coconut_dispatch_event format.
      auto payloadStr = msg.payload.is_null() ? "{}" : msg.payload.dump();
      auto jsName = escapeJsSingleQuotedString(msg.name);
      auto jsPayload = escapeJsSingleQuotedString(payloadStr);
      auto script = std::format(
          "globalThis.__coconut_dispatch_event('{}', '{}');",
          jsName, jsPayload);
      webview_eval(m_webview, script.c_str());
      break;
    }
    case rpc::Type::kReturn:
    case rpc::Type::kError: {
      // Promise resolution via __coconut_rpc_receive.
      auto json = msg.toJson().dump();
      auto escaped = escapeJsSingleQuotedString(json);
      auto script = std::format(
          "globalThis.__coconut_rpc_receive('{}');", escaped);
      webview_eval(m_webview, script.c_str());
      break;
    }
    case rpc::Type::kCall: {
      // Invoke a named JS function with the payload.
      auto payloadStr = msg.payload.dump();
      auto script = std::format(
          "globalThis['{}']({});", msg.name, payloadStr);
      webview_eval(m_webview, script.c_str());
      break;
    }
    case rpc::Type::kReady: {
      // kReady is baked into the webview_init() script — no need to eval.
      break;
    }
  }
}

void WebviewTransport::setMessageCallback(transport::MessageCallback cb) {
  m_callback = std::move(cb);
}

// static
void WebviewTransport::static_on_rpc(const char* id, const char* req,
                                      void* arg) {
  auto* self = static_cast<WebviewTransport*>(arg);
  if (!self || !self->m_callback) return;

  // req is a JSON array of arguments: [msgJson]
  // Parse it to extract the RPC message JSON string.
  std::string msgJson;
  try {
    auto args = nlohmann::json::parse(req);
    if (args.is_array() && args.size() >= 1 && args[0].is_string()) {
      msgJson = args[0].get<std::string>();
    }
  } catch (...) {
    return;
  }

  if (msgJson.empty()) return;

  auto msg = rpc::Message::fromJson(msgJson);
  self->m_callback(msg);
}

} // namespace coconut::bridge
