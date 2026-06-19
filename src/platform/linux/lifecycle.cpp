/// Linux lifecycle hooks using GTK3 signal connections.
///
/// Connects to GtkWindow signals to detect resize, focus, and blur events.
/// Dispatches events to both JS (bridge::emitToJS) and Lua (bridge::dispatchEventToLua).
///
/// Uses g_signal_connect with GCallback handlers. The GtkWindow pointer is
/// retrieved from the webview via webview_get_window().

#include "lifecycle.h"
#include "../../app.h"
#include "../../bridge.h"
#include "../../debug.h"

#include <webview/webview.h>

#include <format>
#include <string>

#include <gtk/gtk.h>

namespace coconut::lifecycle {

static App* s_app = nullptr;

/// Keep track of signal handler IDs so we can disconnect later.
struct SignalHandlers {
  gulong configure_event_id   = 0;
  gulong focus_in_event_id    = 0;
  gulong focus_out_event_id   = 0;
  gulong destroy_id           = 0;
};

static SignalHandlers s_handles{};

/// Get the GtkWindow from the webview handle.
static GtkWindow* getWindow() {
  if (!s_app || !s_app->webview) return nullptr;
  void* win = webview_get_window(s_app->webview);
  return GTK_WINDOW(win);
}

/// Forward event to both frontend and Lua.
static void dispatch(const std::string& name, nlohmann::json payload) {
  if (!s_app) return;
  bridge::emitToJS(s_app, name, payload);
  bridge::dispatchEventToLua(s_app, name, payload);
}

// ── Signal callbacks ────────────────────────────────────────────────────

extern "C" gboolean coconut_on_configure_event(
    GtkWidget* widget, GdkEventConfigure* event, gpointer user_data) {
  (void)widget; (void)user_data;
  nlohmann::json payload = {
    {"w", event->width},
    {"h", event->height}
  };
  dispatch("resize", payload);
  return FALSE;  // allow event to propagate
}

extern "C" gboolean coconut_on_focus_in_event(
    GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget; (void)event; (void)user_data;
  dispatch("focus", {{"active", true}});
  return FALSE;
}

extern "C" gboolean coconut_on_focus_out_event(
    GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget; (void)event; (void)user_data;
  dispatch("focus", {{"active", false}});
  return FALSE;
}

extern "C" void coconut_on_destroy(GtkWidget* widget, gpointer user_data) {
  (void)widget; (void)user_data;
  debug::info("lifecycle: GtkWindow destroyed");
}

// ── Public platform API ─────────────────────────────────────────────────

void platformRegisterEvents(App* app) {
  if (!app || !app->webview) {
    debug::error("lifecycle: cannot register events — no app/webview");
    return;
  }

  s_app = app;
  GtkWindow* win = getWindow();
  if (!win) {
    debug::error("lifecycle: cannot register events — no GtkWindow");
    return;
  }

  GtkWidget* widget = GTK_WIDGET(win);

  s_handles.configure_event_id = g_signal_connect(
      widget, "configure-event",
      G_CALLBACK(coconut_on_configure_event), nullptr);

  s_handles.focus_in_event_id = g_signal_connect(
      widget, "focus-in-event",
      G_CALLBACK(coconut_on_focus_in_event), nullptr);

  s_handles.focus_out_event_id = g_signal_connect(
      widget, "focus-out-event",
      G_CALLBACK(coconut_on_focus_out_event), nullptr);

  s_handles.destroy_id = g_signal_connect(
      widget, "destroy",
      G_CALLBACK(coconut_on_destroy), nullptr);

  debug::info("registered resize/focus/blur lifecycle signals on GtkWindow");
}

void platformUnregisterEvents() {
  GtkWindow* win = getWindow();
  if (win) {
    GtkWidget* widget = GTK_WIDGET(win);
    if (s_handles.configure_event_id > 0) {
      g_signal_handler_disconnect(widget, s_handles.configure_event_id);
    }
    if (s_handles.focus_in_event_id > 0) {
      g_signal_handler_disconnect(widget, s_handles.focus_in_event_id);
    }
    if (s_handles.focus_out_event_id > 0) {
      g_signal_handler_disconnect(widget, s_handles.focus_out_event_id);
    }
    if (s_handles.destroy_id > 0) {
      g_signal_handler_disconnect(widget, s_handles.destroy_id);
    }
  }

  s_handles = SignalHandlers{};
  s_app = nullptr;
  debug::info("lifecycle: unregistered all signal handlers");
}

} // namespace coconut::lifecycle
