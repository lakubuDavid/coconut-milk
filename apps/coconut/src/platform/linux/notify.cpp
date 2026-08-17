/// Linux notification implementation using libnotify.
///
/// Uses libnotify (notify-send) via GNotification / libnotify C API.
/// Falls back to g_print logging if libnotify is unavailable or fails.

#include "notify.h"

#include <libnotify/notify.h>

#include <string>

namespace coconut::notify {

bool platformNotify(const std::string& title, const std::string& body) {
  if (title.empty() && body.empty()) return false;

  // Static init: call notify_init once per process lifetime.
  // g_get_prgname() may return null; pass a sensible app name.
  static bool notify_inited = false;
  if (!notify_inited) {
    const char* app_name = g_get_prgname();
    if (!app_name) app_name = "coconut";
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
  if (!n) return false;

  notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
  notify_notification_set_timeout(n, NOTIFY_EXPIRES_DEFAULT);

  GError* error = nullptr;
  bool ok = notify_notification_show(n, &error);
  if (error) {
    g_warning("notify_notification_show failed: %s", error->message);
    g_error_free(error);
    ok = false;
  }

  g_object_unref(n);
  return ok;
}

} // namespace coconut::notify
