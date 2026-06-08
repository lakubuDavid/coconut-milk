#include "window.h"

#include <exception>
#include <expected>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

extern "C" {
#include <webui.h>
}

namespace coconut::window {

std::expected<Window *, Error> createWindow(Config *config, size_t window_id) {
  if (window_id == 0) {
    return std::unexpected(Error{.code = ErrorCode::WebUiError, .message = "Invalid WebUI window id"});
  }

  // Keep timeout behavior here to centralize window settings.
  webui_set_timeout(20);

  return new Window{.configs = config, .views = {}, .window_id = window_id};
}

void destroyWindow(Window *window) {
  if (window == nullptr) {
    return;
  }

  // Defensive: webui_destroy may fail on invalid ID, guard it.
  if (window->window_id > 0) {
    webui_destroy(window->window_id);
  }

  // Free any heap-allocated views to prevent memory leaks.
  for (auto& [name, vp] : window->views) {
    (void)name;
    delete vp;
  }
  window->views.clear();

  delete window;
}

void showWindow(Window *window) {
  if (window == nullptr) {
    return;
  }

  // Guard against null config — use hardcoded defaults as fallback.
  int w = 1280;
  int h = 640;
  if (window->configs != nullptr) {
    w = window->configs->window_width;
    h = window->configs->window_height;
  }
  webui_set_size(window->window_id, w, h);

  const std::string& view_name = window->current_view;
  if (view_name.empty()) {
    webui_show_browser(window->window_id, "<html>default View</html>",
                       webui_browser::Webview);
    return;
  }

  // Use find() — never at() which throws on missing key.
  const auto it = window->views.find(view_name);
  if (it == window->views.end() || it->second == nullptr) {
    webui_show_browser(window->window_id, "<html>View not found</html>",
                       webui_browser::Webview);
    return;
  }

  webui_show_browser(window->window_id, it->second->html.c_str(),
                     webui_browser::Webview);
}

void showView(Window *window, std::string name) {
  if (window == nullptr) {
    return;
  }

  // Defensive: only switch if the view exists; otherwise stay on current.
  if (window->views.count(name)) {
    window->current_view = std::move(name);
    showWindow(window);
  }
}

void addView(Window *window, std::string name, View *view) {
  if (window != nullptr && view != nullptr) {
    window->views[name] = view;
  }
}

std::expected<View, Error> createView(std::string pathOrCode, ViewKind kind,
                                      std::optional<ViewConfig> configs) {
  View view{.kind = kind};

  switch (kind) {
  case VIEW_KIND_FILE: {
    try {
      std::stringstream text_content;
      std::ifstream file(pathOrCode);
      if (!file.is_open()) {
        return std::unexpected(
            Error{.code = ErrorCode::MissingFile,
                  .message = "could not open file: " + pathOrCode});
      }
      std::string line;
      while (std::getline(file, line)) {
        text_content << line << '\n';
      }
      view.html = text_content.str();
    } catch (const std::exception &e) {
      return std::unexpected(
          Error{.code = ErrorCode::MissingFile,
                .message = std::string("file read error: ") + e.what()});
    }
    break;
  }
  case VIEW_KIND_HTML:
    view.html = pathOrCode;
    break;
  case VIEW_KIND_URL:
    return std::unexpected(Error{.code = ErrorCode::NotImplementedYet,
                                 .message = "URL views not yet implemented"});
  }

  return view;
}

} // namespace coconut::window
