#include "bridge.h"
#include "app.h"
#include "lua_runtime.h"

// WebUI headers included via bridge.h

#include <algorithm>
#include <format>
#include <memory>
#include <string>

namespace coconut::bridge {

std::expected<State *, Error> create(Config *config) {
  return new State{.configs = config};
}

static std::string escapeLuaString(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

static std::string escapeJsSingleQuotedString(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\'':
        out += "\\'";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Inbound RPC dispatch helpers (bridge owns the dispatch logic)
// ---------------------------------------------------------------------------

/// Route an incoming kEvent RPC message to Lua's coconut.events().
static void dispatchRpcEventToLua(coconut::App* app, const rpc::Message& msg) {
  if (app == nullptr || app->lua_state == nullptr ||
      app->lua_state->lua_state == nullptr || app->lua_state->context == nullptr) {
    return;
  }

  sol::state_view lua(*app->lua_state->lua_state);

  sol::table payloadTable = toTable(lua, msg.payload);

  sol::object coconutObj = lua["coconut"];
  if (!coconutObj.valid() || !coconutObj.is<sol::table>()) {
    return;
  }

  sol::table coconutTbl = coconutObj.as<sol::table>();
  sol::protected_function eventsFn = coconutTbl["events"];
  if (!eventsFn.valid()) {
    return;
  }

  // Call: coconut.events(name, payloadTable, ctx)
  eventsFn(msg.name, payloadTable, app->lua_state->context);
}

/// Route an incoming kCall RPC message to the command registry.
/// Sends a kReturn or kError response through the transport.
static void dispatchRpcCallToLua(coconut::App* app, const rpc::Message& msg) {
  if (app == nullptr || app->commands == nullptr) {
    return;
  }

  // Look up handler in the command registry.
  auto it = app->commands->handlers.find(msg.name);
  if (it == app->commands->handlers.end()) {
    rpc::Message err;
    err.type = rpc::Type::kError;
    err.id   = msg.id;
    err.payload = {{"code", "CommandNotFound"},
                   {"message", "No handler for '" + msg.name + "'"}};
    rpcSend(app, err);
    return;
  }

  // Invoke the registered Lua function.
  const sol::protected_function& fn = it->second;

  sol::state_view lua(*app->lua_state->lua_state);
  sol::table paramsTable = toTable(lua, msg.payload);

  auto result = fn(paramsTable, app->lua_state->context);

  rpc::Message reply;
  reply.id = msg.id;

  if (result.valid()) {
    reply.type = rpc::Type::kReturn;
    sol::object val = result;
    if (val.is<sol::table>()) {
      reply.payload = toJson(val.as<sol::table>());
    } else if (val.is<std::string>()) {
      reply.payload = val.as<std::string>();
    } else if (val.is<long long>()) {
      reply.payload = val.as<long long>();
    } else if (val.is<double>()) {
      reply.payload = val.as<double>();
    } else if (val.is<bool>()) {
      reply.payload = val.as<bool>();
    }
  } else {
    reply.type = rpc::Type::kError;
    sol::error err = result;
    reply.payload = {{"code", "LuaError"}, {"message", err.what()}};
  }

  rpcSend(app, reply);
}

// ---------------------------------------------------------------------------
// Remaining legacy helpers (callLua, callJS using raw WebUI)
// ---------------------------------------------------------------------------

void emitToLua(coconut::App *app, std::string eventName,
               nlohmann::json payload) {
  if (app == nullptr || app->lua_state == nullptr ||
      app->lua_state->lua_state == nullptr) {
    return;
  }

  const auto payloadJson = payload.dump();
  const auto luaEventName = escapeLuaString(eventName);
  const auto luaPayloadJson = escapeLuaString(payloadJson);

  const std::string script = std::format(
      R"(if coconut and coconut.events then coconut.events("{}", "{}", ctx) end)",
      luaEventName, luaPayloadJson);

  app->lua_state->lua_state->do_string(script);
}

void emitToJS(coconut::App *app, std::string eventName,
               nlohmann::json payload) {
  // Route through the transport as an RPC event.
  rpc::Message msg;
  msg.type    = rpc::Type::kEvent;
  msg.name    = std::move(eventName);
  msg.payload = std::move(payload);
  rpcSend(app, msg);
}

void emitToJS(window::Window* window, std::string eventName,
              nlohmann::json payload) {
  if (window == nullptr || window->window_id == 0) {
    return;
  }

  // Low-level fallback: send directly when no App* is available.
  // TODO: route through transport once window owns a transport ref.
  const auto payloadJson = payload.dump();
  const auto jsName = escapeJsSingleQuotedString(eventName);
  const auto jsPayloadJson = escapeJsSingleQuotedString(payloadJson);

  const std::string script = std::format(
      "globalThis.__coconut_dispatch_event('{}', '{}');", jsName,
      jsPayloadJson);

  webui_run(window->window_id, script.c_str());
}

void callLua(coconut::App *app, std::string functionName,
              nlohmann::json payload) {
  if (app == nullptr || app->lua_state == nullptr ||
      app->lua_state->lua_state == nullptr) {
    return;
  }

  sol::state &lua = *app->lua_state->lua_state;
  sol::protected_function fn = lua[functionName];
  if (!fn.valid()) {
    return;
  }

  sol::object arg = functionName == "coconut.events"
                        ? sol::make_object(lua, payload.dump())
                        : toTable(lua, payload);

  auto result = fn(arg);
  if (!result.valid()) {
    sol::error err = result;
    (void)err;
  }
}

void callJS(coconut::App *app, std::string functionName,
            nlohmann::json payload) {
  if (app == nullptr || app->window == nullptr) {
    return;
  }
  if (app->window->window_id == 0) {
    return;
  }

  const std::string payloadStr = payload.dump();
  const std::string script = std::format(
      "globalThis['{}']({});", functionName, payloadStr);

  webui_run(app->window->window_id, script.c_str());
}

// ---------------------------------------------------------------------------
// WebUI transport — wraps webui_run / webui_bind behind Transport interface
// ---------------------------------------------------------------------------

namespace {

/// Concrete transport that sends/receives RPC messages over WebUI's
/// webui_run()/webui_bind() mechanism.
///
/// Outbound flow:  send(RpcMessage) → translates to JS function calls
/// Inbound flow:   webui_bind callback → constructs RpcMessage → m_callback(msg)
class WebuiTransport : public transport::Transport {
public:
  WebuiTransport(size_t win_id, coconut::App* app)
      : m_win_id(win_id), m_app(app) {
    // ── Inbound event channel ──
    // JS calls:  __coconut_emit(name, payloadJson)
    webui_bind(win_id, "__coconut_emit", &static_on_message);
    webui_set_context(win_id, "__coconut_emit", this);

    // ── Inbound command channel ──
    // JS calls:  __coconut_call(name, payloadJson)
    // The handler invokes the Lua command and returns the result synchronously
    // via webui_return(), resolving the JS Promise.
    webui_bind(win_id, "__coconut_call", &static_on_call);
    webui_set_context(win_id, "__coconut_call", this);

    // Inject JS adapter that maps __coconut_js_listener → __coconut_emit.
    const char* adapter =
        "(function(){\n"
        "  if (!globalThis.__coconut_js_listener) {\n"
        "    globalThis.__coconut_js_listener = function(name, payloadJson) "
        "{\n"
        "      return globalThis.__coconut_emit(name, payloadJson);\n"
        "    };\n"
        "  }\n"
        "})();";
    webui_run(win_id, adapter);
  }

  ~WebuiTransport() override = default;

  void send(const rpc::Message& msg) override {
    switch (msg.type) {
      case rpc::Type::kEvent: {
        // Keep the existing __coconut_dispatch_event format.
        auto payloadStr = msg.payload.is_null()
                              ? "{}"
                              : msg.payload.dump();
        auto jsName = escapeJsSingleQuotedString(msg.name);
        auto jsPayload = escapeJsSingleQuotedString(payloadStr);
        auto script = std::format(
            "globalThis.__coconut_dispatch_event('{}', '{}');",
            jsName, jsPayload);
        webui_run(m_win_id, script.c_str());
        break;
      }
      case rpc::Type::kReturn:
      case rpc::Type::kError: {
        // Promise resolution: send full RPC JSON to a dedicated receiver.
        auto json = msg.toJson().dump();
        auto escaped = escapeJsSingleQuotedString(json);
        auto script = std::format(
            "globalThis.__coconut_rpc_receive('{}');", escaped);
        webui_run(m_win_id, script.c_str());
        break;
      }
      case rpc::Type::kCall: {
        // Invoke a named JS function with the payload.
        auto payloadStr = msg.payload.dump();
        auto script = std::format(
            "globalThis['{}']({});", msg.name, payloadStr);
        webui_run(m_win_id, script.c_str());
        break;
      }
      case rpc::Type::kReady: {
        webui_run(m_win_id, "globalThis.__coconut_bridge_ready();");
        break;
      }
    }
  }

  void setMessageCallback(transport::MessageCallback cb) override {
    m_callback = std::move(cb);
  }

private:
  /// Static WebUI callback — reads __coconut_emit args, constructs kEvent RpcMessage.
  static void static_on_message(webui_event_t* e) {
    void* raw = webui_get_context(e);
    auto* self = static_cast<WebuiTransport*>(raw);
    if (!self || !self->m_callback) {
      return;
    }

    const char* eventNameC = webui_get_string(e);
    const char* payloadJsonC = webui_get_string_at(e, 1);

    rpc::Message msg;
    msg.type = rpc::Type::kEvent;
    msg.name = eventNameC ? eventNameC : "";
    if (payloadJsonC) {
      try {
        msg.payload = nlohmann::json::parse(payloadJsonC);
      } catch (...) {
        msg.payload = nlohmann::json::object();
      }
    }

    self->m_callback(msg);
  }

  /// Static WebUI callback — reads __coconut_call args, invokes Lua command,
  /// calls webui_return with the result envelope so the JS Promise resolves.
  static void static_on_call(webui_event_t* e) {
    void* raw = webui_get_context(e);
    auto* self = static_cast<WebuiTransport*>(raw);
    if (!self || !self->m_app) {
      return;
    }

    const char* nameC = webui_get_string(e);
    const char* payloadJsonC = webui_get_string_at(e, 1);

    const std::string name = nameC ? nameC : "";
    nlohmann::json payload = nlohmann::json::object();
    if (payloadJsonC) {
      try {
        payload = nlohmann::json::parse(payloadJsonC);
      } catch (...) {}
    }

    coconut::App* app = self->m_app;

    // Build response envelope: { ok:true, data } or { ok:false, error }.
    auto makeError = [](const std::string& code,
                        const std::string& msg) -> nlohmann::json {
      return {{"ok", false},
              {"error", {{"code", code}, {"message", msg}}}};
    };

    // Look up the command.
    auto respond = [e](const nlohmann::json& env) {
      std::string s = env.dump();
      webui_return_string(e, s.c_str());
    };

    if (!app->commands) {
      respond(makeError("BridgeError", "No command registry"));
      return;
    }
    auto it = app->commands->handlers.find(name);
    if (it == app->commands->handlers.end()) {
      respond(makeError("CommandNotFound", "No handler for '" + name + "'"));
      return;
    }

    // Invoke the Lua command.
    if (!app->lua_state || !app->lua_state->lua_state ||
        !app->lua_state->context) {
      respond(makeError("BridgeError", "No Lua runtime"));
      return;
    }

    sol::state_view lua(*app->lua_state->lua_state);
    sol::table paramsTable = toTable(lua, payload);

    auto result = it->second(paramsTable, app->lua_state->context);

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

  size_t m_win_id;
  coconut::App* m_app = nullptr;
  transport::MessageCallback m_callback;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Transport factory and public bridge API
// ---------------------------------------------------------------------------

void createTransport(coconut::App* app) {
  if (app == nullptr || app->window == nullptr ||
      app->bridge_state == nullptr) {
    return;
  }
  const size_t win_id = app->window->window_id;
  if (win_id == 0) {
    return;
  }

  // Create the WebUI transport with app pointer for __coconut_call binding.
  auto* t = new WebuiTransport(win_id, app);
  app->bridge_state->transport = t;

  // Register the bridge callback: incoming RpcMessages are dispatched to Lua.
  t->setMessageCallback(
      [app](const rpc::Message& msg) {
        switch (msg.type) {
          case rpc::Type::kEvent:
            dispatchRpcEventToLua(app, msg);
            break;
          case rpc::Type::kCall:
            dispatchRpcCallToLua(app, msg);
            break;
          default:
            // kReady, kReturn, kError not expected on inbound for now
            break;
        }
      });
}

/// Signal to the frontend that the bridge is ready.
/// Called after the window is shown and the JS page has loaded.
void signalReady(coconut::App* app) {
  rpc::Message ready;
  ready.type = rpc::Type::kReady;
  rpcSend(app, ready);
}

void rpcSend(coconut::App* app, const rpc::Message& msg) {
  if (app == nullptr || app->bridge_state == nullptr ||
      app->bridge_state->transport == nullptr) {
    return;
  }
  app->bridge_state->transport->send(msg);
}

// ---------------------------------------------------------------------------
// JSON <-> Lua conversion helpers
// ---------------------------------------------------------------------------

static sol::object jsonToLua(sol::state_view lua, const nlohmann::json& v) {
  if (v.is_null()) {
    return sol::lua_nil;
  }
  if (v.is_boolean()) {
    return sol::make_object(lua, v.get<bool>());
  }
  if (v.is_number_integer()) {
    return sol::make_object(lua, v.get<long long>());
  }
  if (v.is_number_unsigned()) {
    return sol::make_object(lua, v.get<unsigned long long>());
  }
  if (v.is_number_float()) {
    return sol::make_object(lua, v.get<double>());
  }
  if (v.is_string()) {
    return sol::make_object(lua, v.get<std::string>());
  }

  if (v.is_array()) {
    sol::table t = lua.create_table();
    std::size_t i = 1;
    for (const auto& item : v) {
      t[i++] = jsonToLua(lua, item);
    }
    return t;
  }

  if (v.is_object()) {
    sol::table t = lua.create_table();
    for (auto it = v.begin(); it != v.end(); ++it) {
      t[it.key()] = jsonToLua(lua, it.value());
    }
    return t;
  }

  return sol::lua_nil;
}

sol::table toTable(sol::state_view lua, const nlohmann::json& json) {
  sol::object obj = jsonToLua(lua, json);
  if (obj.is<sol::table>()) {
    return obj.as<sol::table>();
  }

  sol::table t = lua.create_table();
  t["value"] = obj;
  return t;
}

sol::table toTable(sol::state_view lua, const std::string& jsonStr) {
  nlohmann::json parsed = nlohmann::json::parse(jsonStr);
  return toTable(lua, parsed);
}

static nlohmann::json luaToJsonValue(const sol::object& obj);

static nlohmann::json luaToJsonTable(const sol::table& t) {
  bool looksArray = true;
  std::size_t maxIndex = 0;
  std::size_t count = 0;

  for (auto&& kv : t) {
    const sol::object& k = kv.first;
    if (!k.is<int>() && !k.is<long long>() && !k.is<unsigned int>() &&
        !k.is<unsigned long long>()) {
      looksArray = false;
      break;
    }

    std::size_t idx = static_cast<std::size_t>(k.as<long long>());
    maxIndex = std::max(maxIndex, idx);
    ++count;
  }

  if (looksArray && maxIndex >= count) {
    nlohmann::json arr = nlohmann::json::array();
    for (std::size_t i = 0; i < maxIndex; ++i) {
      arr.push_back(nullptr);
    }

    for (auto&& kv : t) {
      const sol::object& k = kv.first;
      std::size_t idx = static_cast<std::size_t>(k.as<long long>());
      const sol::object& v = kv.second;
      arr[idx - 1] = luaToJsonValue(v);
    }
    return arr;
  }

  nlohmann::json obj = nlohmann::json::object();
  for (auto&& kv : t) {
    const sol::object& k = kv.first;
    const sol::object& v = kv.second;

    std::string key;
    if (k.is<std::string>()) {
      key = k.as<std::string>();
    } else {
      if (k.is<long long>() || k.is<int>()) {
        key = std::to_string(static_cast<long long>(k.as<long long>()));
      } else {
        key = "";
      }
    }

    if (!key.empty()) {
      obj[key] = luaToJsonValue(v);
    }
  }
  return obj;
}

static nlohmann::json luaToJsonValue(const sol::object& obj) {
  if (!obj.valid()) {
    return nullptr;
  }
  if (obj == sol::lua_nil) {
    return nullptr;
  }
  if (obj.is<bool>()) {
    return obj.as<bool>();
  }
  if (obj.is<std::string>()) {
    return obj.as<std::string>();
  }
  if (obj.is<int>()) {
    return obj.as<int>();
  }
  if (obj.is<long long>()) {
    return obj.as<long long>();
  }
  if (obj.is<double>()) {
    return obj.as<double>();
  }
  if (obj.is<float>()) {
    return static_cast<double>(obj.as<float>());
  }
  if (obj.is<sol::table>()) {
    return luaToJsonTable(obj.as<sol::table>());
  }

  return obj.as<std::string>();
}

nlohmann::json toJson(const sol::table& table) {
  return luaToJsonTable(table);
}

void destroy(State *state) {
  if (state == nullptr) {
    return;
  }
  delete state->transport; // WebuiTransport
  state->transport = nullptr;
  delete state;
}

} // namespace coconut::bridge
