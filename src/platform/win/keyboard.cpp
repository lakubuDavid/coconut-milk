/// Win32 keyboard hook implementation using WH_KEYBOARD_LL low-level hook.
///
/// Converts virtual key codes to Coconut Milk combo strings (e.g., "mod+s")
/// and dispatches to the registered callback.

#include "keyboard.h"
#include "../../debug.h"
#include <windows.h>
#include <string>
#include <cstdint>

namespace coconut {
namespace platform {

static HHOOK g_keyboard_hook = nullptr;
static KeyEventCallback g_callback = nullptr;
static void* g_userdata = nullptr;

/// Map a virtual key code to a string name.
static std::string VKeyToString(DWORD vk) {
  switch (vk) {
    case VK_BACK:      return "backspace";
    case VK_TAB:       return "tab";
    case VK_CLEAR:     return "clear";
    case VK_RETURN:    return "enter";
    case VK_SHIFT:     return "shift";
    case VK_CONTROL:   return "ctrl";
    case VK_MENU:      return "alt";
    case VK_PAUSE:     return "pause";
    case VK_CAPITAL:   return "capslock";
    case VK_ESCAPE:    return "escape";
    case VK_SPACE:     return "space";
    case VK_PRIOR:     return "pageup";
    case VK_NEXT:      return "pagedown";
    case VK_END:       return "end";
    case VK_HOME:      return "home";
    case VK_LEFT:      return "left";
    case VK_UP:        return "up";
    case VK_RIGHT:     return "right";
    case VK_DOWN:      return "down";
    case VK_SNAPSHOT:  return "printscreen";
    case VK_INSERT:    return "insert";
    case VK_DELETE:    return "delete";
    case VK_OEM_PLUS:  return "+";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_COMMA: return ",";

    // Number keys
    case '0': return "0";
    case '1': return "1";
    case '2': return "2";
    case '3': return "3";
    case '4': return "4";
    case '5': return "5";
    case '6': return "6";
    case '7': return "7";
    case '8': return "8";
    case '9': return "9";

    // Letter keys
    case 'A': return "a";
    case 'B': return "b";
    case 'C': return "c";
    case 'D': return "d";
    case 'E': return "e";
    case 'F': return "f";
    case 'G': return "g";
    case 'H': return "h";
    case 'I': return "i";
    case 'J': return "j";
    case 'K': return "k";
    case 'L': return "l";
    case 'M': return "m";
    case 'N': return "n";
    case 'O': return "o";
    case 'P': return "p";
    case 'Q': return "q";
    case 'R': return "r";
    case 'S': return "s";
    case 'T': return "t";
    case 'U': return "u";
    case 'V': return "v";
    case 'W': return "w";
    case 'X': return "x";
    case 'Y': return "y";
    case 'Z': return "z";

    // Function keys
    case VK_F1:  return "f1";
    case VK_F2:  return "f2";
    case VK_F3:  return "f3";
    case VK_F4:  return "f4";
    case VK_F5:  return "f5";
    case VK_F6:  return "f6";
    case VK_F7:  return "f7";
    case VK_F8:  return "f8";
    case VK_F9:  return "f9";
    case VK_F10: return "f10";
    case VK_F11: return "f11";
    case VK_F12: return "f12";
    case VK_F13: return "f13";
    case VK_F14: return "f14";
    case VK_F15: return "f15";
    case VK_F16: return "f16";
    case VK_F17: return "f17";
    case VK_F18: return "f18";
    case VK_F19: return "f19";
    case VK_F20: return "f20";

    default: return "";
  }
}

// Low-level keyboard hook procedure
static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wparam, LPARAM lparam) {
  if (nCode < 0 || !g_callback) {
    return CallNextHookEx(nullptr, nCode, wparam, lparam);
  }

  // Only process keydown events
  if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
    KBDLLHOOKSTRUCT* khs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);

    // Build combo string
    std::string combo;

    // Check modifiers
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
      combo += "ctrl+";
    }
    if (GetAsyncKeyState(VK_MENU) & 0x8000) {
      combo += "alt+";
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
      combo += "shift+";
    }
    if (GetAsyncKeyState(VK_LWIN) & 0x8000 ||
        GetAsyncKeyState(VK_RWIN) & 0x8000) {
      combo += "mod+";
    }

    std::string key = VKeyToString((DWORD)khs->vkCode);
    if (key.empty()) {
      return CallNextHookEx(nullptr, nCode, wparam, lparam);
    }

    combo += key;

    bool handled = false;
    bool consumed = g_callback(combo, &handled, g_userdata);

    if (consumed || handled) {
      return 1; // Block the event
    }
  }

  return CallNextHookEx(nullptr, nCode, wparam, lparam);
}

void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata) {
  g_callback = cb;
  g_userdata = userdata;

  // WH_KEYBOARD_LL requires the message pump to be on the same thread
  // that installed the hook. This works because our main thread runs
  // the Windows message loop via webview_run().
  g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                       KeyboardHookProc,
                                       GetModuleHandleW(nullptr),
                                       0);

  if (g_keyboard_hook) {
    debug::info("Win32: registered low-level keyboard hook");
  } else {
    debug::warn("Win32: failed to register keyboard hook");
  }

  (void)app_ptr;
}

void unregisterKeyboardMonitor() {
  if (g_keyboard_hook) {
    UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
    debug::info("Win32: unregistered keyboard hook");
  }

  g_callback = nullptr;
  g_userdata = nullptr;
}

} // namespace platform
} // namespace coconut
