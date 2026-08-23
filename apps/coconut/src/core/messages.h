#ifndef CORE_MESSAGES_H
#define CORE_MESSAGES_H

#include "rpc_envelope.h"

#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace coconut::core {

  // ── Wire envelope (JS ↔ C++) ────────────────────────────────────────

  enum class RpcType {
    kCall,    // Request a command call (JS → C++)
    kReturn,  // Successful response to a call (C++ → JS)
    kError,   // Error response to a call (C++ → JS)
    kEvent,   // One-way fire-and-forget (either direction)
    kReady,   // Bridge readiness handshake (JS → C++)
  };

  /// Returns a string for a type (for JSON serialization).
  inline const char* typeToString(RpcType t) {
    switch (t) {
      case RpcType::kCall:
        return "call";
      case RpcType::kReturn:
        return "return";
      case RpcType::kError:
        return "error";
      case RpcType::kEvent:
        return "event";
      case RpcType::kReady:
        return "ready";
    }
    return "unknown";
  }

  /// Parse a type from its string name.
  inline RpcType typeFromString(const std::string& s) {
    if (s == "call")
      return RpcType::kCall;
    if (s == "return")
      return RpcType::kReturn;
    if (s == "error")
      return RpcType::kError;
    if (s == "event")
      return RpcType::kEvent;
    if (s == "ready")
      return RpcType::kReady;
    return RpcType::kEvent;  // fallback
  }

  /// The canonical JS↔C++ RPC envelope carried by the transport.
  struct JsRPCMessage {
    RpcType        type = RpcType::kEvent;
    std::string    id;       ///< empty for fire-and-forget (event, ready)
    std::string    name;     ///< command name, event name, or binding name
    nlohmann::json payload;  ///< params, result, or error details

    /// Serialize to a JSON object.
    nlohmann::json toJson() const {
      nlohmann::json j;
      j["type"] = typeToString(type);
      if (!id.empty())
        j["id"] = id;
      if (!name.empty())
        j["name"] = name;
      if (!payload.is_null())
        j["payload"] = payload;
      return j;
    }

    /// Deserialize from a JSON object.
    static JsRPCMessage fromJson(const nlohmann::json& j) {
      JsRPCMessage msg;
      if (j.is_null())
        return msg;
      msg.type = typeFromString(j.value("type", std::string("event")));
      msg.id   = j.value("id", std::string());
      msg.name = j.value("name", std::string());
      if (j.contains("payload"))
        msg.payload = j["payload"];
      return msg;
    }

    /// Deserialize from a JSON string.
    static JsRPCMessage fromJson(const std::string& s) {
      try {
        return fromJson(nlohmann::json::parse(s));
      } catch (...) {
        return JsRPCMessage{};
      }
    }
  };

  // ── Dispatcher messages ─────────────────────────────────────────────

  struct LifecycleMessage {
    std::string ViewName;
    std::string EventName;
  };

  struct CommandCallMessage {
    std::string    CommandName;
    nlohmann::json Args;
  };

  struct CommandResultMessage {
    std::string    CommandName;
    nlohmann::json Result;
  };

  struct JsCallMessage {
    /// RPC envelope forwarded to the Webview via the transport (e.g. a kCall or
    /// kEvent). The Dispatcher sends it directly — no raw webview_eval.
    JsRPCMessage Message;
  };

  using DispatchMessage = std::variant<LifecycleMessage, CommandCallMessage, JsCallMessage>;

  // ── Worker messages ─────────────────────────────────────────────────

  using RequestId = std::uint64_t;

  struct PromiseMessage {
    RequestId      id;
    std::string    command;
    nlohmann::json args;
  };

  struct ResolveMessage {
    RequestId      id;
    nlohmann::json result;
  };

  struct RejectMessage {
    RequestId   id;
    std::string error;
  };

  using WorkerInput  = std::variant<PromiseMessage>;
  using WorkerOutput = std::variant<ResolveMessage, RejectMessage>;

}  // namespace coconut::core

#endif  // CORE_MESSAGES_H
