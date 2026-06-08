#ifndef COCONUT_WEBVIEW_TRANSPORT_H
#define COCONUT_WEBVIEW_TRANSPORT_H

/// @file webview_transport.h
///
/// Concrete transport implementation using the webview library's native
/// WKWebView bridge (macOS) or WebView2 (Windows).
///
/// Inbound:  webview_bind("__coconut_rpc", callback) — JS → C++
/// Outbound: webview_eval(w, js)                    — C++ → JS
/// Init:     webview_init(w, js)                    — injected before page load

#include "transport.h"

#include <webview/webview.h>

#include <string>

namespace coconut::bridge {

/// Transport backed by webview's native WKWebView / WebView2.
///
/// Usage:
///   1. Create with a webview_t handle.
///   2. setMessageCallback() to receive inbound RPC messages.
///   3. send() dispatches outbound messages to the frontend.
///   4. The webview_run() loop keeps the bridge alive.
class WebviewTransport : public transport::Transport {
public:
  /// Create the transport and bind JS entry points.
  /// The webview must already exist and be initialized.
  /// @param w The webview instance (owned by the caller).
  /// @param coconut_js The full Coconut JS runtime source to inject
  ///                   via webview_init().
  WebviewTransport(webview_t w, const std::string& coconut_js);

  ~WebviewTransport() override;

  /// Send an RPC message to the frontend via webview_eval().
  void send(const rpc::Message& msg) override;

  /// Register the callback for messages received from the frontend.
  void setMessageCallback(transport::MessageCallback cb) override;

  /// Return the webview handle (for window management).
  webview_t handle() const { return m_webview; }

private:
  /// Static webview bind callback — dispatches to instance.
  static void static_on_rpc(const char* id, const char* req, void* arg);

  webview_t m_webview;
  transport::MessageCallback m_callback;
  bool m_inited = false;
};

} // namespace coconut::bridge

#endif // COCONUT_WEBVIEW_TRANSPORT_H
