/// Linux GtkEventControllerKey keyboard monitor.
///
/// Monitors key press events on the GtkWindow and forwards them
/// through the hybrid dispatch chain. Platform-level keybinds
/// (like mod+h to hide) are consumed before reaching the webview.

#include "keyboard.h"
#include "../../app.h"

#include <webview/webview.h>

#include <gtk/gtk.h>

#include <format>
#include <string>
#include <unordered_map>
#include <vector>

namespace coconut::platform {

static GtkEventController* s_keyController = nullptr;
static KeyEventCallback s_callback = nullptr;
static void* s_userdata = nullptr;

// ── Key name mapping ────────────────────────────────────────────────────

/// Map GDK keyvals to human-readable key names matching the JS conventions
/// used by coconut.keybind().
struct KeyMapping {
  guint keyval;
  const char* name;
};

static const KeyMapping s_keyMap[] = {
  {GDK_KEY_Escape,       "escape"},
  {GDK_KEY_Tab,          "tab"},
  {GDK_KEY_ISO_Left_Tab, "tab"},
  {GDK_KEY_Return,       "enter"},
  {GDK_KEY_KP_Enter,     "enter"},
  {GDK_KEY_space,        "space"},
  {GDK_KEY_BackSpace,    "backspace"},
  {GDK_KEY_Delete,       "delete"},
  {GDK_KEY_Home,         "home"},
  {GDK_KEY_End,          "end"},
  {GDK_KEY_Page_Up,      "pageup"},
  {GDK_KEY_Page_Down,    "pagedown"},
  {GDK_KEY_Insert,       "insert"},

  // Arrow keys
  {GDK_KEY_Up,           "up"},
  {GDK_KEY_Down,         "down"},
  {GDK_KEY_Left,         "left"},
  {GDK_KEY_Right,        "right"},

  // Function keys
  {GDK_KEY_F1,  "f1"},  {GDK_KEY_F2,  "f2"},
  {GDK_KEY_F3,  "f3"},  {GDK_KEY_F4,  "f4"},
  {GDK_KEY_F5,  "f5"},  {GDK_KEY_F6,  "f6"},
  {GDK_KEY_F7,  "f7"},  {GDK_KEY_F8,  "f8"},
  {GDK_KEY_F9,  "f9"},  {GDK_KEY_F10, "f10"},
  {GDK_KEY_F11, "f11"}, {GDK_KEY_F12, "f12"},
};

/// Convert a GDK keyval to a lowercase key name string.
static std::string keyvalToName(guint keyval) {
  // Check special keys first
  for (const auto& mapping : s_keyMap) {
    if (mapping.keyval == keyval) {
      return mapping.name;
    }
  }

  // Printable ASCII: convert to lowercase string
  gchar unichar[8] = {};
  if (g_unichar_isprint(keyval)) {
    gunichar c = gdk_keyval_to_unicode(keyval);
    if (c) {
      unichar[g_unichar_to_utf8(g_unichar_tolower(c), unichar)] = '\0';
      return std::string(unichar);
    }
  }

  // Fallback: use GDK's name
  const gchar* name = gdk_keyval_name(keyval);
  if (name) {
    std::string s(name);
    // Convert to lowercase for consistency
    for (auto& ch : s) ch = static_cast<char>(std::tolower(ch));
    return s;
  }

  return "unknown";
}

/// Build a combo string from GDK modifier state + keyval.
static std::string buildCombo(guint keyval, GdkModifierType state) {
  std::vector<std::string> parts;

  // GDK modifier order: ctrl, alt, shift, mod (super/hyper)
  // macOS uses "mod" for Cmd. Linux uses GDK_SUPER_MASK for the Super (Win) key.
  if (state & GDK_CONTROL_MASK)  parts.push_back("ctrl");
  if (state & GDK_MOD1_MASK)     parts.push_back("alt");     // Alt key
  if (state & GDK_SHIFT_MASK)    parts.push_back("shift");
  if (state & GDK_SUPER_MASK)    parts.push_back("mod");     // Super/Windows key

  std::string keyName = keyvalToName(keyval);
  parts.push_back(keyName);

  std::string combo;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) combo += "+";
    combo += parts[i];
  }
  return combo;
}

/// Check if this is a platform-level combo (consumed before reaching keybind registry).
static bool isPlatformCombo(const std::string& combo) {
  return (combo == "mod+h") ||
         (combo == "mod+m") ||
         (combo == "mod+tab") ||
         (combo == "mod+shift+tab") ||
         (combo == "mod+space");
}

// ── GTK signal handler ──────────────────────────────────────────────────

extern "C" gboolean coconut_on_key_pressed(
    GtkEventControllerKey* controller, guint keyval, guint keycode,
    GdkModifierType state, gpointer user_data) {
  (void)controller; (void)keycode; (void)user_data;

  std::string combo = buildCombo(keyval, state);

  // 1. Platform-level combos — always consume
  if (isPlatformCombo(combo)) {
    g_message("[coconut.keyboard] consumed platform combo: %s", combo.c_str());
    return TRUE; // stop propagation
  }

  // 2. Forward to C++ callback for keybind registry check
  bool handled = false;
  bool consumed = false;
  if (s_callback) {
    consumed = s_callback(combo, &handled, s_userdata);
  }

  if (consumed) return TRUE;
  return FALSE; // allow event to propagate to webview
}

// ── Public API ──────────────────────────────────────────────────────────

void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata) {
  if (s_keyController) {
    g_warning("[coconut.keyboard] monitor already registered");
    return;
  }

  s_callback = cb;
  s_userdata = userdata;

  // Get the GtkWindow from the App's webview
  if (!app_ptr) {
    g_warning("[coconut.keyboard] cannot register — null app");
    return;
  }
  auto* app = static_cast<coconut::App*>(app_ptr);
  if (!app->webview) {
    g_warning("[coconut.keyboard] cannot register — no webview");
    return;
  }

  void* win_raw = webview_get_window(app->webview);
  if (!win_raw) {
    g_warning("[coconut.keyboard] cannot register — no GtkWindow");
    return;
  }
  GtkWidget* window = GTK_WIDGET(win_raw);

  // Create and attach the key event controller
  s_keyController = gtk_event_controller_key_new();
  gtk_event_controller_set_propagation_phase(
      s_keyController, GTK_PHASE_CAPTURE);  // capture phase: intercept before webview

  g_signal_connect(s_keyController, "key-pressed",
      G_CALLBACK(coconut_on_key_pressed), nullptr);

  gtk_widget_add_controller(window, s_keyController);

  g_message("[coconut.keyboard] GtkEventControllerKey registered");
}

void unregisterKeyboardMonitor() {
  if (s_keyController) {
    g_object_unref(s_keyController);
    s_keyController = nullptr;
  }
  s_callback = nullptr;
  s_userdata = nullptr;
  g_message("[coconut.keyboard] monitor removed");
}

} // namespace coconut::platform
