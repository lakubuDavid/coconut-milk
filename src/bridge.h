#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include "error.h"
#include "rpc_envelope.h"
#include "transport.h"

#include <nlohmann/json.hpp>

#include <expected>
#include <memory>
#include <string_view>

#include <sol/sol.hpp>
#include <sol/table.hpp>

namespace coconut {
  class App;
  namespace bridge {

    struct State {
      Config*                   configs    = nullptr;
      transport::Transport*     transport  = nullptr; ///< owned, deleted in destroy()
    };

    std::expected<State*, Error> create(Config* config);
    void                         destroy(State* state);

    void emitToLua(coconut::App* app,std::string eventName, nlohmann::json payload);
    void emitToJS(coconut::App* app,std::string eventName,nlohmann::json payload);

    void callLua(coconut::App* app,std::string fuctionName,nlohmann::json payload);
    void callJS(coconut::App* app,std::string functionName,nlohmann::json payload);

    /// Create a transport for the given app and store it on the bridge State.
    /// Registers the frontend-side binding and injects the JS adapter.
    /// Inbound messages are routed to the bridge's internal callback.
    void createTransport(App* app);

    /// Send an RPC message through the bridge state's transport.
    void rpcSend(App* app, const rpc::Message& msg);

    /// Signal the frontend that the bridge is ready.
    /// Called after the window is shown and JS has loaded.
    void signalReady(App* app);

    // Convert json object/array to a lua table.
    // Requires a Lua state to allocate the sol::table.
    sol::table toTable(sol::state_view lua, const nlohmann::json& json);
    sol::table toTable(sol::state_view lua, const std::string& json);

    // Convert lua table to a json value.
    nlohmann::json toJson(const sol::table& table);

  // ── Utility: escape a string for embedding in a single-quoted JS string ──
  inline std::string escapeJsSingleQuotedString(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '\\': out += "\\\\"; break;
        case '\'': out += "\\'"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:   out += c;      break;
      }
    }
    return out;
  }

  }  // namespace bridge
}  // namespace coconut

#endif  // BRIDGE_H
