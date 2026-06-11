#ifndef COCONUT_TRANSPORT_H
#define COCONUT_TRANSPORT_H

/// @file transport.h
///
/// Abstract transport interface for JS ↔ C++ RPC messages.
///
/// Concrete implementations wrap platform-specific mechanisms:
///   - WebviewTransport: webview's native WKScriptMessageHandler
///
/// The transport simply sends/receives RpcMessage envelopes.
/// It does not interpret the message contents.

#include "rpc_envelope.h"

#include <functional>

namespace coconut::transport {

/// Callback invoked when a message arrives from the frontend.
using MessageCallback = std::function<void(const rpc::Message&)>;

/// Abstract transport for JS ↔ C++ RPC messages.
///
/// Usage:
///   1. Create a concrete transport (e.g. WebviewTransport).
///   2. Set the message callback via setMessageCallback().
///   3. Call send() to deliver messages to the frontend.
///   4. Incoming messages arrive on the registered callback.
///
class Transport {
public:
  virtual ~Transport() = default;

  /// Send an RPC message to the frontend.
  virtual void send(const rpc::Message& msg) = 0;

  /// Register the callback for messages received from the frontend.
  /// Only one callback can be registered at a time.
  /// Set an empty callback to unregister.
  virtual void setMessageCallback(MessageCallback cb) = 0;
};

} // namespace coconut::transport

#endif // COCONUT_TRANSPORT_H
