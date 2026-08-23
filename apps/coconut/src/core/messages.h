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

  struct JsCallMessage {
    /// RPC envelope forwarded to the Webview via the transport (e.g. a kCall or
    /// kEvent). The Dispatcher sends it directly — no raw webview_eval.
    rpc::Message Message;
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

  // Note: rpc::Message (the JS↔C++ wire envelope) is defined in rpc_envelope.h.
  // The legacy stringly-typed dispatch envelope (Message / MessageKind) now
  // lives in dispatch.h, used only by the pre-core dispatch/bg_runtime path.

}  // namespace coconut::core

#endif  // CORE_MESSAGES_H
