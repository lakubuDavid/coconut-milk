#include "bridge.h"
#include "app.h"
#include "lua_runtime.h"
#include "webview_transport.h"
#include "embeds/coconut_embed.h"

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

// escapeJsSingleQuotedString is now defined inline in bridge.h

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
  if (app == nullptr || app->window == nullptr || app->webview == nullptr) {
    return;
  }

  const std::string payloadStr = payload.dump();
  const std::string script = std::format(
      "globalThis['{}']({});", functionName, payloadStr);

  webview_eval(app->webview, script.c_str());
}

// ---------------------------------------------------------------------------
// Webview transport — wraps webview_bind / webview_eval behind Transport interface
// ---------------------------------------------------------------------------

static std::string decodeEmbed() {
  size_t len = sizeof(coconut_js_embed);
  if (len > 0 && coconut_js_embed[len - 1] == 0) len -= 1;
  return std::string(
      reinterpret_cast<const char*>(coconut_js_embed), len);
}

void createTransport(coconut::App* app) {
  if (app == nullptr || app->webview == nullptr ||
      app->bridge_state == nullptr) {
    return;
  }

  // Decode the embedded Coconut JS runtime.
  std::string coconut_js = decodeEmbed();

  // Append shims for old __coconut_call / __coconut_emit names that
  // coconut.ts still references, and auto-fire the bridge-ready signal.
  coconut_js += R"(
// Shim: forward __coconut_call through the __coconut_rpc webview binding.
// webview_bind creates an async function; webview_return() resolves the promise.
globalThis.__coconut_call = async function(name, payloadJson) {
  var msg = JSON.stringify({ type: "call", name: name, payload: JSON.parse(payloadJson) });
  var result = await globalThis.__coconut_rpc(msg);
  // __coconut_rpc returns the parsed envelope object; re-stringify for coconut.call()
  return JSON.stringify(result);
};
globalThis.__coconut_emit = async function(name, payloadJson) {
  var msg = JSON.stringify({ type: "event", name: name, payload: JSON.parse(payloadJson) });
  await globalThis.__coconut_rpc(msg);
};
globalThis.__coconut_bridge_ready();
)";

  // Create the webview transport, which calls:
  //   webview_init()  — injects Coconut JS runtime (fires on next page load)
  //   webview_bind()  — registers __coconut_rpc for inbound messages
  // The transport handles dispatch internally (via App* reference).
  auto* t = new WebviewTransport(app->webview, app, coconut_js);
  app->bridge_state->transport = t;
}

/// Signal to the frontend that the bridge is ready.
/// With webview this is a no-op — kReady is baked into the init script
/// passed to webview_init() in createTransport().
void signalReady(coconut::App* app) {
  (void)app;
  // kReady fires automatically via webview_init script.
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
