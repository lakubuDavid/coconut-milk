/// Linux notification implementation using libnotify.
///
/// Uses libnotify (notify-send) via GNotification / libnotify C API.
/// Falls back to g_print logging if libnotify is unavailable or fails.

#include "notify.h"

#include <libnotify/notify.h>

#include <string>

namespace coconut::notify {

  namespace {

    /// libnotify's notify_notification_show() is asynchronous: the notification
    /// object must stay alive until the server closes it. Unref in the "closed"
    /// handler instead of right after show() — an immediate unref races with the
    /// GDBus worker thread (TSan data races in the notify tests).
    void onNotificationClosed(NotifyNotification* n, gpointer /*user_data*/) {
      g_object_unref(n);
    }

  }  // namespace

  bool platformNotify(const std::string& title, const std::string& body) {
    if (title.empty() && body.empty())
      return false;

    // Static init: call notify_init once per process lifetime.
    // g_get_prgname() may return null; pass a sensible app name.
    static bool notify_inited = false;
    if (!notify_inited) {
      const char* app_name = g_get_prgname();
      if (!app_name)
        app_name = "coconut";
      notify_inited = notify_init(app_name);
      if (!notify_inited) {
        g_warning("notify_init failed — notifications unavailable");
        return false;
      }
    }

    NotifyNotification* n = notify_notification_new(
        title.c_str(),
        body.c_str(),
        nullptr  // icon (use default)
    );
    if (!n)
      return false;

    notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, NOTIFY_EXPIRES_DEFAULT);

    // Keep the notification alive until the server closes it (see above).
    g_signal_connect(n, "closed", G_CALLBACK(onNotificationClosed), nullptr);

    GError* error = nullptr;
    bool    ok    = notify_notification_show(n, &error);
    if (error) {
      g_warning("notify_notification_show failed: %s", error->message);
      g_error_free(error);
      ok = false;
    }

    if (!ok) {
      // show() failed synchronously — no async operation pending, safe to unref.
      g_object_unref(n);
    }
    // On success, n is owned by the "closed" handler and unref'd there.

    return ok;
  }

}  // namespace coconut::notify
