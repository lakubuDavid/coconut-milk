#include "window.h"

#include "../dispatch.h"
#include "../platform/darwin/window_handle.h"
#include "modules/thread_kind.h"
#include "webview/api.h"
#include "webview/types.h"

#include <functional>
#include <future>
#include <sol/state.hpp>
#include <sol/table.hpp>
#include <sol/types.hpp>
#include <string>

namespace coconut::modules {

  namespace {
    webview_t g_target_webview = nullptr;
  }  // namespace

  void setWindowTarget(webview_t wv) {
    g_target_webview = wv;
  }

  void init_window(sol::state& lua, ThreadKind kind) {
    webview_t        wv       = g_target_webview;
    const ThreadKind thisKind = kind;

    sol::table coconut = lua["coconut"].get_or_create<sol::table>();
    sol::table win     = lua.create_table();

    // ── Shared plumbing ───────────────────────────────────────────────
    //
    // apply(op): on Main, run the native op inline; on Background,
    // marshal it onto the main run loop (fire-and-forget). Returns an
    // {ok=true} table, or {ok=false,error=...} when no target is set.

    auto apply = [thisKind](std::function<void(webview_t)> op, sol::this_state s) -> sol::table {
      sol::state_view lv(s);
      sol::table      t = lv.create_table();
      // g_target_webview is read at CALL time, not registration time —
      // init_window may run before setWindowTarget() during startup.
      webview_t wv = g_target_webview;
      if (wv == nullptr) {
        t["ok"]    = false;
        t["error"] = "no window target (setWindowTarget not called)";
        return t;
      }
      if (thisKind == ThreadKind::Main) {
        op(wv);
      } else {
        dispatch::post([op, wv] { op(wv); });
      }
      t["ok"] = true;
      return t;
    };

    // ── Mutations (async from workers) ────────────────────────────────

    win.set_function("setTitle", [apply](std::string title, sol::this_state s) {
      return apply([title](webview_t w) { window::platformSetWindowTitle(w, title); }, s);
    });

    win.set_function("setResizable", [apply](bool on, sol::this_state s) {
      return apply([on](webview_t w) { window::platformSetResizable(w, on); }, s);
    });

    win.set_function("setMinimumSize", [apply](int w, int h, sol::this_state s) {
      return apply([w, h](webview_t v) { window::platformSetMinimumWindowSize(v, w, h); }, s);
    });

    win.set_function("setMaximumSize", [apply](int w, int h, sol::this_state s) {
      return apply([w, h](webview_t v) { window::platformSetMaximumWindowSize(v, w, h); }, s);
    });

    win.set_function("setSize", [apply](int w, int h, sol::this_state s) {
      return apply([w, h](webview_t v) { webview_set_size(v, w, h, WEBVIEW_HINT_NONE); }, s);
    });

    win.set_function("setPosition", [apply](int x, int y, sol::this_state s) {
      return apply([x, y](webview_t v) { window::platformSetWindowPosition(v, x, y); }, s);
    });

    // ── Window-state conveniences (delegate to platform fns too) ─────

    win.set_function("minimize", [apply](sol::this_state s) {
      return apply([](webview_t v) { window::platformMinimizeWindow(v); }, s);
    });

    win.set_function("maximize", [apply](sol::this_state s) {
      return apply([](webview_t v) { window::platformMaximizeWindow(v); }, s);
    });

    win.set_function("toggleFullscreen", [apply](sol::this_state s) {
      return apply([](webview_t v) { window::platformToggleFullscreen(v); }, s);
    });

    // ── Queries (synchronous round-trip from workers) ──────────────────

    win.set_function("getPosition", [thisKind](sol::this_state s) -> sol::table {
      webview_t       wv = g_target_webview;  // lazy lookup (see apply note)
      sol::state_view lv(s);
      sol::table      t = lv.create_table();
      int             x = 0, y = 0;

      auto readPos = [wv](int& x, int& y) {
        if (wv != nullptr) {
          window::platformGetWindowPosition(wv, x, y);
        }
      };

      if (thisKind == ThreadKind::Main) {
        readPos(x, y);
      } else {
        // Sync round-trip: run the read on the main thread during its next
        // drain and block here until it lands. Safe because the main loop
        // never blocks on workers.
        std::packaged_task<void()> task([&] { readPos(x, y); });
        auto                       fut = task.get_future();
        dispatch::post([&task] { task(); });
        fut.wait();
      }
      t["x"]  = x;
      t["y"]  = y;
      t["ok"] = true;
      return t;
    });

    coconut["window"] = win;
  }

}  // namespace coconut::modules
