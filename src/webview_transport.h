#ifndef COCONUT_WEBVIEW_TRANSPORT_H
#define COCONUT_WEBVIEW_TRANSPORT_H

/// @file webview_transport.h
///
/// Concrete transport implementation using the webview library's native
/// WKWebView bridge (macOS) or WebView2 (Windows).
///
/// Inbound:  webview_bind("__coconut_rpc", callback) — JS → C++
///           Handles kCall (invoke Lua command + webview_return) and
///           kEvent (dispatch to Lua + webview_return).
/// Outbound: webview_eval(w, js)                    — C++ → JS
/// Init:     webview_init(w, js)                    — injected before page load
///
/// The transport holds an App* reference so it can invoke Lua commands
/// directly and respond via webview_return(), which resolves the Promise
/// created by webview_bind() on the JS side.

#include "app_fwd.h"
#include "transport.h"

#include <webview/webview.h>

#include <string>

namespace coconut::bridge {

/// Transport backed by webview's native WKWebView / WebView2.
///
/// Inbound messages arrive via the `__coconut_rpc` webview binding.
/// The callback parses the RPC envelope and dispatches:
///   - kCall → command registry → result sent via webview_return()
///   - kEvent → Lua event handler → then webview_return() with undefined
class WebviewTransport : public transport::Transport {
public:
  /// Create the transport, inject the Coconut JS runtime, and bind
  /// the inbound RPC channel.
  WebviewTransport(webview_t w, coconut::App* app,
                   const std::string& coconut_js);

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

  /// Handle an inbound kCall: invoke Lua command, respond via webview_return.
  void handleCall(const char* id, const rpc::Message& msg);

  /// Handle an inbound kEvent: dispatch to Lua, respond via webview_return.
  void handleEvent(const char* id, const rpc::Message& msg);

  webview_t m_webview;
  coconut::App* m_app = nullptr;
  transport::MessageCallback m_callback;
};

} // namespace coconut::bridge

#endif // COCONUT_WEBVIEW_TRANSPORT_H
