#include "webview_transport.h"
#include "app.h"
#include "bridge.h" // for escapeJsSingleQuotedString, toTable, toJson
#include "commands.h"
#include "debug.h"
#include "lua_runtime.h"

#include <nlohmann/json.hpp>

#include <format>
#include <iostream>
#include <string>

namespace coconut::bridge {

WebviewTransport::WebviewTransport(webview_t w, coconut::App* app,
                                   const std::string& coconut_js)
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
  webview_bind(w, "__coconut_list_views",
               &WebviewTransport::static_list_views, this);
}

WebviewTransport::~WebviewTransport() {
  m_webview = nullptr;
  m_app = nullptr;
}

void WebviewTransport::send(const rpc::Message& msg) {
  if (!m_webview) return;

  switch (msg.type) {
    case rpc::Type::kEvent: {
      auto payloadStr = msg.payload.is_null() ? "{}" : msg.payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
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
      auto json = msg.toJson().dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
      auto escaped = escapeJsSingleQuotedString(json);
      auto script = std::format(
          "globalThis.__coconut_rpc_receive('{}');", escaped);
      webview_eval(m_webview, script.c_str());
      break;
    }
    case rpc::Type::kCall: {
      auto payloadStr = msg.payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
      auto script = std::format(
          "globalThis['{}']({});", msg.name, payloadStr);
      webview_eval(m_webview, script.c_str());
      break;
    }
    case rpc::Type::kReady: {
      // Handled by webview_init() — no eval needed.
      break;
    }
  }
}

void WebviewTransport::setMessageCallback(transport::MessageCallback cb) {
  m_callback = std::move(cb);
}

// ---------------------------------------------------------------------------
// Inbound RPC: called when JS invokes window.__coconut_rpc(...)
// ---------------------------------------------------------------------------

// static
void WebviewTransport::static_on_rpc(const char* id, const char* req,
                                      void* arg) {
  auto* self = static_cast<WebviewTransport*>(arg);
  if (!self || !self->m_webview) return;

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
  if (msgJson.empty()) return;

  auto msg = rpc::Message::fromJson(msgJson);

  switch (msg.type) {
    case rpc::Type::kCall:
      self->handleCall(id, msg);
      break;
    case rpc::Type::kEvent:
      self->handleEvent(id, msg);
      break;
    default:
      // Unknown type — resolve with undefined.
      webview_return(self->m_webview, id, 0, "");
      break;
  }
}

// static
void WebviewTransport::static_list_views(const char* id, const char* req,
                                          void* arg) {
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
  webview_return(self->m_webview, id, 0, names.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace).c_str());
}

void WebviewTransport::handleCall(const char* id, const rpc::Message& msg) {
  std::string payloadPreview;
  try {
    payloadPreview = msg.payload.dump();
  } catch (...) {
    payloadPreview = "[invalid json]";
  }
  debug::info(std::format("handleCall: name='{}' payload={}", msg.name, payloadPreview));

  // Build the full envelope so webview's onReply parses it back to an object.
  // The JS shim for __coconut_call re-stringifies it for coconut.call().
  auto respond = [this, id](nlohmann::json envelope) {
    std::string result;
    try {
      result = envelope.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (const std::exception& e) {
      debug::error(std::format("handleCall: JSON dump failed: {}", e.what()));
      nlohmann::json fallback;
      fallback["ok"] = false;
      fallback["error"] = {{"code", "BridgeError"},
                           {"message", "JSON serialization failed"}};
      result = fallback.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }
    webview_return(m_webview, id, 0, result.c_str());
  };

  auto makeError = [](const std::string& code,
                      const std::string& message) -> nlohmann::json {
    return {{"ok", false},
            {"error", {{"code", code}, {"message", message}}}};
  };

  if (!m_app || !m_app->commands) {
    respond(makeError("BridgeError", "No command registry"));
    return;
  }

  auto it = m_app->commands->handlers.find(msg.name);
  if (it == m_app->commands->handlers.end()) {
    respond(makeError("CommandNotFound", "No handler for '" + msg.name + "'"));
    return;
  }

  if (!m_app->lua_state || !m_app->lua_state->lua_state ||
      !m_app->lua_state->context) {
    respond(makeError("BridgeError", "No Lua runtime"));
    return;
  }

  sol::state_view lua(*m_app->lua_state->lua_state);
  sol::table paramsTable = toTable(lua, msg.payload);

  auto result = it->second(paramsTable, m_app->lua_state->context);

  nlohmann::json envelope;
  if (result.valid()) {
    envelope["ok"] = true;
    sol::object val = result;
    if (!val.valid() || val == sol::lua_nil) {
      envelope["data"] = nullptr;
    } else if (val.is<std::string>()) {
      envelope["data"] = val.as<std::string>();
    } else if (val.is<bool>()) {
      envelope["data"] = val.as<bool>();
    } else if (val.is<int>() || val.is<long long>()) {
      envelope["data"] = val.as<long long>();
    } else if (val.is<double>() || val.is<float>()) {
      envelope["data"] = val.as<double>();
    } else if (val.is<sol::table>()) {
      envelope["data"] = toJson(val.as<sol::table>());
    } else {
      envelope["data"] = nullptr;
    }
  } else {
    envelope["ok"] = false;
    sol::error err = result;
    envelope["error"] = {{"code", "LuaError"}, {"message", err.what()}};
  }

  respond(envelope);
}

void WebviewTransport::handleEvent(const char* id, const rpc::Message& msg) {
  std::string eventPayloadPreview;
  try {
    eventPayloadPreview = msg.payload.dump();
  } catch (...) {
    eventPayloadPreview = "[invalid json]";
  }
  debug::info(std::format("handleEvent: name='{}' payload={}", msg.name, eventPayloadPreview));

  // Dispatch to Lua's coconut.events(name, payload, ctx).
  if (m_app && m_app->lua_state && m_app->lua_state->lua_state &&
      m_app->lua_state->context) {
    sol::state_view lua(*m_app->lua_state->lua_state);
    sol::table payloadTable = toTable(lua, msg.payload);

    sol::object coconutObj = lua["coconut"];
    if (coconutObj.valid() && coconutObj.is<sol::table>()) {
      sol::table coconutTbl = coconutObj.as<sol::table>();
      sol::protected_function eventsFn = coconutTbl["events"];
      if (eventsFn.valid()) {
        eventsFn(msg.name, payloadTable, m_app->lua_state->context);
      }
    }
  }

  // Resolve the JS promise with undefined.
  webview_return(m_webview, id, 0, "");
}

} // namespace coconut::bridge
