#include "webview_transport.h"
#include "app.h"
#include "bridge.h"
#include "commands.h"
#include "common.h"
#include "debug.h"
#include "main_runtime.h"

#include <nlohmann/json.hpp>

#include <format>
#include <iostream>
#include <string>

namespace coconut::bridge {

  WebviewTransport::WebviewTransport(webview_t w, coconut::App* app, const std::string& coconut_js)
      : m_webview(w), m_app(app) {
    if (!w) {
      debug::error("WebviewTransport: null handle");
      return;
    }

    // Inject the Coconut JS runtime before any page loads.
    // This includes the coconut object, __coconut_bridge_ready(), and
    // shims for __coconut_call / __coconut_emit that delegate to __coconut_rpc.
    if (!coconut_js.empty()) {
      webview_init(w, coconut_js.c_str());
    }

    // Bind the inbound RPC channel.
    // JS calls:  window.__coconut_rpc(msgJson)
    // webview:   req = JSON.stringify([msgJson])
    //            promise resolved by webview_return() called from handler
    webview_bind(w, "__coconut_rpc", &WebviewTransport::static_on_rpc, this);

    // Bind the list-views query — returns view names as a JSON string array.
    // JS calls:  let names = await window.__coconut_list_views()
    webview_bind(w, "__coconut_list_views", &WebviewTransport::static_list_views, this);
  }

  WebviewTransport::~WebviewTransport() {
    m_webview = nullptr;
    m_app     = nullptr;
  }

  void WebviewTransport::send(const coconut::core::JsRPCMessage& msg) {
    if (!m_webview)
      return;

    switch (msg.type) {
      case coconut::core::RpcType::kEvent: {
        auto payloadStr =
            msg.payload.is_null()
                ? "{}"
                : msg.payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        auto jsName    = common::escapeString(msg.name, '\'');
        auto jsPayload = common::escapeString(payloadStr, '\'');
        auto script =
            std::format("globalThis.__coconut_dispatch_event('{}', '{}');", jsName, jsPayload);
        webview_eval(m_webview, script.c_str());
        break;
      }
      case coconut::core::RpcType::kReturn:
      case coconut::core::RpcType::kError: {
        auto json    = msg.toJson().dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        auto escaped = common::escapeString(json, '\'');
        auto script  = std::format("globalThis.__coconut_rpc_receive('{}');", escaped);
        webview_eval(m_webview, script.c_str());
        break;
      }
      case coconut::core::RpcType::kCall: {
        auto payloadStr =
            msg.payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        auto script = std::format("globalThis['{}']({});", msg.name, payloadStr);
        webview_eval(m_webview, script.c_str());
        break;
      }
      case coconut::core::RpcType::kReady: {
        // Handled by webview_init() — no eval needed.
        break;
      }
    }
  }

  void WebviewTransport::setMessageCallback(transport::MessageCallback cb) {
    m_callback = std::move(cb);
  }

  void WebviewTransport::eval(const std::string& js) {
    if (m_webview != nullptr) {
      webview_eval(m_webview, js.c_str());
    }
  }

  // ---------------------------------------------------------------------------
  // Inbound RPC: called when JS invokes window.__coconut_rpc(...)
  // ---------------------------------------------------------------------------

  // static
  void WebviewTransport::static_on_rpc(const char* id, const char* req, void* arg) {
    auto* self = static_cast<WebviewTransport*>(arg);
    if (!self || !self->m_webview)
      return;

    // Parse req = JSON.stringify([msgJson])
    std::string msgJson;
    try {
      auto args = nlohmann::json::parse(req);
      if (args.is_array() && args.size() >= 1 && args[0].is_string()) {
        msgJson = args[0].get<std::string>();
      }
    } catch (const std::exception& e) {
      debug::error(std::format("static_on_rpc: failed to parse JSON: {}", e.what()));
      return;
    }
    if (msgJson.empty())
      return;

    auto msg = coconut::core::JsRPCMessage::fromJson(msgJson);

    // Core-path routing: when a message callback is registered (the core
    // Bridge/Dispatcher wiring), hand the envelope over and return. Promise
    // resolution for kCall moves to the async protocol (__coconut_rpc_receive
    // with the message id) — webview_return is not used on this path.
    if (self->m_callback) {
      self->m_callback(msg);
      return;
    }

    // No callback registered — inbound traffic has no route. Resolve the
    // bind promise so JS doesn't hang on an eternally-pending call.
    debug::warn("static_on_rpc: no message callback registered; dropping message");
    webview_return(self->m_webview, id, 0, "");
  }

  // static
  void WebviewTransport::static_list_views(const char* id, const char* req, void* arg) {
    auto* self = static_cast<WebviewTransport*>(arg);
    if (!self || !self->m_webview) {
      return;
    }

    nlohmann::json names = nlohmann::json::array();
    if (self->m_app && self->m_app->window) {
      for (const auto& [name, _] : self->m_app->window->views) {
        names.push_back(name);
      }
    }

    // Return the array — webview expects a JSON string.
    webview_return(
        self->m_webview,
        id,
        0,
        names.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace).c_str()
    );
  }

}  // namespace coconut::bridge
