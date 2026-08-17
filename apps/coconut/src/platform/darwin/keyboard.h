#ifndef COCONUT_PLATFORM_KEYBOARD_H
#define COCONUT_PLATFORM_KEYBOARD_H

#include <functional>
#include <string>

namespace coconut {
  struct App;
  namespace platform {

    /// Callback invoked by the NSEvent monitor for each keyDown event.
    /// Returns true if the event was consumed (should not reach webview).
    /// Sets *handled to true if a registered keybind handler fired.
    using KeyEventCallback = bool (*)(const std::string& combo,
                                      bool* handled,
                                      void* userdata);

    /// Register the NSEvent keyDown monitor.
    void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata);

    /// Remove the NSEvent monitor.
    void unregisterKeyboardMonitor();
  }
}

#endif // COCONUT_PLATFORM_KEYBOARD_H
