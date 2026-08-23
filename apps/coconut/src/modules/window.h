#ifndef COCONUT_MODULES_WINDOW_H
#define COCONUT_MODULES_WINDOW_H

#include <sol/sol.hpp>
#include "thread_kind.h"
#include "webview/api.h"  // webview_t

namespace coconut::modules {

  /// Register coconut.window.* — live window mutations from any thread.
  ///
  ///   Main kind      : calls run inline on the calling (main) thread.
  ///   Background kind: calls marshal onto the main run loop via
  ///                    dispatch::post(); getPosition() blocks until the
  ///                    next drain completes the round-trip.
  ///
  /// The native target must be set before Background registration via
  /// setWindowTarget(webview_t) — typically app->webview right after
  /// window creation.
  void init_window(sol::state& lua, ThreadKind kind);

  /// Set the webview handle the module operates on. Call once during app
  /// startup, before worker pools are built.
  void setWindowTarget(webview_t wv);

}  // namespace coconut::modules

#endif  // COCONUT_MODULES_WINDOW_H
