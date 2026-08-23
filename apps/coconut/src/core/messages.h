#ifndef CORE_MESSAGES_H
#define CORE_MESSAGES_H

#include "rpc_envelope.h"

#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace coconut::core {

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

  struct EvalJSMessage {
    std::string JsCode;
  };

  using DispatchMessage = std::variant<LifecycleMessage, CommandCallMessage, EvalJSMessage>;

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

  // ── Legacy dispatch (pipe-separated payload) ────────────────────────

  enum class MessageKind : uint8_t {
    EvalJS,
    LifecycleEvent,
    CommandCall,
    CommandResult,
  };

  struct Message {
    MessageKind kind;
    std::string payload;
  };

  // Note: rpc::Message is defined in rpc_envelope.h

}  // namespace coconut::core

#endif  // CORE_MESSAGES_H
