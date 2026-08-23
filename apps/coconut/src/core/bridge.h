#ifndef CORE_BRIDGE_H
#define CORE_BRIDGE_H

#include "error.h"
#include "messages.h"   // CommandCallMessage, JsRPCMessage
#include "transport.h"  // transport::Transport

#include <nlohmann/json.hpp>
#include <sol/state_view.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace coconut::core {

  class Bridge;  // forward decl — defined below

  /// Fluent builder for Bridge. Each `with*` step configures a dependency;
  /// `build()` validates and constructs the Bridge instance.
  struct BridgeBuilder {
    std::shared_ptr<transport::Transport> TransportPtr;  ///< shared with Dispatcher
    std::optional<sol::state_view>        LuaState;      ///< set via withLuaState

    /// Sync executor: runs main-thread-only commands on the spot.
    /// Return nullopt to fall through (command not owned by the main thread).
    std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)>
        SyncExecutor;

    /// Resolves the active view name used as the dispatch target for events.
    std::function<std::string()> TargetResolver;

    /// Share the webview transport (also used by the Dispatcher).
    BridgeBuilder& withTransport(std::shared_ptr<transport::Transport> transport);
    /// Bind the main-thread Lua state for event dispatch and command routing.
    BridgeBuilder& withLuaState(sol::state_view lua);
    /// Register the sync executor for main-thread-only commands.
    BridgeBuilder& withSyncExecutor(
        std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)> fn
    );
    /// Register a resolver for the active view name (event dispatch target).
    BridgeBuilder& withTargetResolver(std::function<std::string()> fn);
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

    /// Executes main-thread-only commands synchronously. Returns nullopt for
    /// commands it doesn't own (routed to workers via _CommandCallHandler).
    std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)>
        _SyncExecutor;

    /// Provides the active view name as the event dispatch target.
    std::function<std::string()> _TargetResolver;

    Bridge(
        std::shared_ptr<transport::Transport> transport,
        sol::state_view                       lua,
        std::function<std::optional<CommandResult>(const std::string&, const nlohmann::json&)>
                                     syncExecutor,
        std::function<std::string()> targetResolver
    );

   public:
    /// Begin a fluent builder for a new Bridge.
    static BridgeBuilder builder();

    // ── Inbound RPC (driven by the transport's callback) ────────────

    /// Single inbound entry point: routes kEvent → emitToLua and kCall →
    /// sync executor (main-thread-only commands) or the worker path via
    /// forwardCommandCall. Replies use the async envelope protocol
    /// (__coconut_rpc_receive) — webview_return is never used.
    void onInbound(const JsRPCMessage& msg);

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
    void rpcSend(const JsRPCMessage& msg);
  };

}  // namespace coconut::core

#endif  // CORE_BRIDGE_H
