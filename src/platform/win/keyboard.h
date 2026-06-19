#ifndef COCONUT_PLATFORM_WIN_KEYBOARD_H
#define COCONUT_PLATFORM_WIN_KEYBOARD_H

#include <functional>
#include <string>

namespace coconut {
  struct App;

  namespace platform {
    /// Callback invoked by the keyboard hook for each key event.
    /// Returns true if the event was consumed.
    /// Sets *handled to true if a registered keybind handler fired.
    using KeyEventCallback = bool (*)(const std::string& combo,
                                      bool* handled,
                                      void* userdata);

    /// Register a low-level keyboard hook (WH_KEYBOARD_LL).
    void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata);

    /// Remove the keyboard hook.
    void unregisterKeyboardMonitor();
  }
}

#endif // COCONUT_PLATFORM_WIN_KEYBOARD_H
