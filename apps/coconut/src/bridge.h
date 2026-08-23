#ifndef BRIDGE_H
#define BRIDGE_H

#include "common.h"
#include "config.h"
#include "error.h"
#include "rpc_envelope.h"
#include "store.h"
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
      Config*               configs   = nullptr;
      transport::Transport* transport = nullptr;  ///< owned, deleted in destroy()
      store::Store*         store     = nullptr;  ///< owned, deleted in destroy()
    };

    std::expected<State*, Error> create(Config* config);
    void                         destroy(State* state);

    /// Dispatch a named event + JSON payload through coconut._dispatch().
    void dispatchEventToLua(
        coconut::App* app, const std::string& name, const nlohmann::json& payload
    );

    void emitToJS(coconut::App* app, std::string eventName, nlohmann::json payload);
    void callJS(coconut::App* app, std::string functionName, nlohmann::json payload);

    /// Create a transport for the given app and store it on the bridge State.
    /// Registers the frontend-side binding and injects the JS adapter.
    /// Inbound messages are routed to the bridge's internal callback.
    void createTransport(App* app);

    /// Send an RPC message through the bridge state's transport.
    void rpcSend(App* app, const rpc::Message& msg);

    /// Signal the frontend that the bridge is ready.
    /// Called after the window is shown and JS has loaded.
    void signalReady(App* app);

  }  // namespace bridge
}  // namespace coconut

#endif  // BRIDGE_H
