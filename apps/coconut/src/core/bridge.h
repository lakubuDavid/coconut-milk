#ifndef CORE_BRIDGE_H
#define CORE_BRIDGE_H

#include "error.h"
#include "messages.h"      // CommandCallMessage
#include "rpc_envelope.h"  // rpc::Message
#include "transport.h"     // transport::Transport

#include <nlohmann/json.hpp>
#include <sol/state_view.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace coconut::core {

  class Bridge;  // forward decl — defined below

  /// Fluent builder for Bridge. Each `with*` step configures a dependency;
  /// `build()` validates and constructs the Bridge instance.
  struct BridgeBuilder {
    std::shared_ptr<transport::Transport> TransportPtr;        ///< shared with Dispatcher
    sol::state_view                       LuaState = nullptr;  ///< borrowed (App-owned)

    /// Share the webview transport (also used by the Dispatcher).
    BridgeBuilder& withTransport(std::shared_ptr<transport::Transport> transport);
    /// Bind the main-thread Lua state for event dispatch and command routing.
    BridgeBuilder& withLuaState(sol::state_view lua);
    /// Validate configuration and construct the Bridge.
    std::expected<std::unique_ptr<Bridge>, coconut::Error> build();
  };

  /// The bridge between C++ and the webview/Lua runtime.
  ///
  /// Owns the transport; terminates inbound RPC and provides the outbound
  /// facade. Command *calls* are forwarded to an injected handler (the
  /// Dispatcher) — the Bridge deliberately holds no WorkerPool dependency.
  ///
  /// Created via `Bridge::builder().withTransport(...).withLuaState(...).build()`.
  class Bridge {
    friend struct BridgeBuilder;

   private:
    std::shared_ptr<transport::Transport>   _Transport;     ///< shared with Dispatcher
    sol::state_view                         _MainLuaState;  ///< borrowed (App-owned)
    std::function<void(CommandCallMessage)> _CommandCallHandler;

    Bridge(std::shared_ptr<transport::Transport> transport, sol::state_view lua);

   public:
    /// Begin a fluent builder for a new Bridge.
    static BridgeBuilder builder();

    // ── Inbound RPC (driven by the transport's callback) ─────────────

    /// Dispatch an event through `coconut._dispatch` on the main Lua thread.
    void emitToLua(const std::string& name, const nlohmann::json& payload);

    /// Forward a command call to the registered handler (e.g. the Dispatcher).
    /// No-op if no handler is set — keeps the Bridge unaware of the WorkerPool.
    void forwardCommandCall(const CommandCallMessage& msg);

    /// Register the sink for inbound command calls (typically the Dispatcher).
    void setCommandCallHandler(std::function<void(CommandCallMessage)> handler);

    // ── Outbound: C++ → JS ────────────────────────────────────────────

    /// Emit an event to the frontend via the transport (rpc::Type::kEvent).
    void emitToJS(const std::string& eventName, const nlohmann::json& payload);

    /// Send a raw RPC envelope to the frontend via the transport.
    void rpcSend(const rpc::Message& msg);
  };

}  // namespace coconut::core

#endif  // CORE_BRIDGE_H
