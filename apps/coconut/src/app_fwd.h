#ifndef COCONUT_APP_FWD_H
#define COCONUT_APP_FWD_H

/// Forward declaration of App to break circular includes.
/// (app.h → bridge.h → webview_transport.h → app.h)
namespace coconut {
  struct App;
}

#endif // COCONUT_APP_FWD_H
