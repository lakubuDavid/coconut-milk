#ifndef COCONUT_PLATFORM_LINUX_KEYBOARD_H
#define COCONUT_PLATFORM_LINUX_KEYBOARD_H

/// Linux keyboard event monitoring via GtkEventControllerKey.
///
/// Replaces macOS NSEvent monitors with GTK3's event controller API.
/// The GtkEventControllerKey is attached to the GtkWindow and forwards
/// key events to the hybrid dispatch chain (platform → Lua → JS).

#include <functional>
#include <string>

namespace coconut {
  struct App;

  namespace platform {

    /// Callback invoked by the key event controller for each key press.
    /// Returns true if the event was consumed (should not reach webview).
    /// Sets *handled to true if a registered keybind handler fired.
    using KeyEventCallback = bool (*)(const std::string& combo,
                                      bool* handled,
                                      void* userdata);

    /// Register the GtkEventControllerKey on the GtkWindow.
    void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata);

    /// Remove the key event controller.
    void unregisterKeyboardMonitor();
  }
}

#endif // COCONUT_PLATFORM_LINUX_KEYBOARD_H
