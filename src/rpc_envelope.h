#ifndef COCONUT_RPC_ENVELOPE_H
#define COCONUT_RPC_ENVELOPE_H

/// @file rpc_envelope.h
///
/// Canonical RPC envelope for JS ↔ C++ message passing.
///
/// Both legacy and native webview transports use the same envelope shape.
/// Only the underlying send/recv mechanism differs.
///
/// Envelope structure:
///   call:   { type: "call",   id: "…", name: "…", payload: {…} }
///   return: { type: "return", id: "…",             payload: <any> }
///   error:  { type: "error",  id: "…",             payload: { code, message, … } }
///   event:  { type: "event",            name: "…", payload: {…} }
///   ready:  { type: "ready" }
///
/// id is empty for fire-and-forget (event, ready).

#include <nlohmann/json.hpp>
#include <string>

namespace coconut::rpc {

/// Canonical RPC message types.
enum class Type {
  kCall,    // Request a command call (JS → C++)
  kReturn,  // Successful response to a call (C++ → JS)
  kError,   // Error response to a call (C++ → JS)
  kEvent,   // One-way fire-and-forget (either direction)
  kReady,   // Bridge readiness handshake (JS → C++)
};

/// Returns a string for a type (for JSON serialization).
inline const char* typeToString(Type t) {
  switch (t) {
    case Type::kCall:   return "call";
    case Type::kReturn: return "return";
    case Type::kError:  return "error";
    case Type::kEvent:  return "event";
    case Type::kReady:  return "ready";
  }
  return "unknown";
}

/// Parse a type from its string name.
inline Type typeFromString(const std::string& s) {
  if (s == "call")   return Type::kCall;
  if (s == "return") return Type::kReturn;
  if (s == "error")  return Type::kError;
  if (s == "event")  return Type::kEvent;
  if (s == "ready")  return Type::kReady;
  return Type::kEvent; // fallback
}

/// The canonical RPC envelope.
///
/// Carries a type discriminator, optional id/name, and a JSON payload.
/// All bridge traffic between C++ and JavaScript uses this shape.
struct Message {
  Type        type    = Type::kEvent;
  std::string id;        ///< empty for fire-and-forget (event, ready)
  std::string name;      ///< command name, event name, or binding name
  nlohmann::json payload; ///< params, result, or error details

  /// Serialize to a JSON object.
  nlohmann::json toJson() const {
    nlohmann::json j;
    j["type"] = typeToString(type);
    if (!id.empty())   j["id"] = id;
    if (!name.empty()) j["name"] = name;
    if (!payload.is_null()) j["payload"] = payload;
    return j;
  }

  /// Deserialize from a JSON object.
  static Message fromJson(const nlohmann::json& j) {
    Message msg;
    if (j.is_null()) return msg;
    msg.type    = typeFromString(j.value("type", std::string("event")));
    msg.id      = j.value("id", std::string());
    msg.name    = j.value("name", std::string());
    if (j.contains("payload")) msg.payload = j["payload"];
    return msg;
  }

  /// Deserialize from a JSON string.
  static Message fromJson(const std::string& s) {
    try {
      return fromJson(nlohmann::json::parse(s));
    } catch (...) {
      return Message{};
    }
  }
};

} // namespace coconut::rpc

#endif // COCONUT_RPC_ENVELOPE_H
