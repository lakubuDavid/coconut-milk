#ifndef COCONUT_PLATFORM_WIN_WINDOW_H
#define COCONUT_PLATFORM_WIN_WINDOW_H

#include "webview/api.h" // webview_t
#include "../../debug.h"

namespace coconut {
  struct Config;

  namespace window {
    /// Apply Windows-native window style (frameless, etc.) — stub.
    inline void platformApplyWindowStyle(webview_t /*wv*/, Config* /*cfg*/) {}

    /// Install WebView2 navigation delegate for external URL interception — stub.
    inline void platformInstallNavDelegate(webview_t /*wv*/) {}

    /// Open devtools (Edge DevTools).
    /// Edge DevTools are enabled via put_AreDevToolsEnabled(TRUE) when
    /// debug=1.  User can press F12 or right-click → Inspect.
    /// Injects a visible hint.
    inline void platformOpenDevTools(webview_t wv) {
      if (!wv) return;
      debug::info("🐚 Debug mode — press F12 or right-click → Inspect to open Edge DevTools.");
      webview_eval(wv, R"JS(
        if (!window.__coconutDevtoolsHint) {
          window.__coconutDevtoolsHint = true;
          var d = document.createElement('div');
          d.id = 'coconut-devtools-hint';
          d.style.cssText = 'position:fixed;top:0;left:0;right:0;z-index:99999;'
            + 'background:#2b2b2b;color:#8bc34a;padding:6px 12px;font:12px/1.4 monospace;'
            + 'border-bottom:1px solid #444;display:flex;align-items:center;gap:8px;';
          d.innerHTML = '<span>🐚</span>'
            + '<span>Debug: press F12 or right-click → Inspect</span>'
            + '<span style="margin-left:auto;cursor:pointer;color:#888" onclick="this.parentNode.remove()">✕</span>';
          document.body && document.body.prepend(d);
        }
      )JS");
    }

    /// Set window background color (0-1 range) — stub.
    inline void platformSetWindowBackgroundColor(webview_t /*wv*/, float, float, float, float) {}
  }
}

#endif // COCONUT_PLATFORM_WIN_WINDOW_H
