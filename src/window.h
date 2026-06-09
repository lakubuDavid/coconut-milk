#ifndef WINDOW_H
#define WINDOW_H

#include "config.h"
#include "error.h"

#include <webview/webview.h>

#include <expected>
#include <map>
#include <optional>
#include <string>

namespace coconut {
namespace window {

enum ViewKind {
  VIEW_KIND_FILE,
  VIEW_KIND_HTML,
  VIEW_KIND_URL,
};

struct ViewConfig {};

struct View {
  ViewKind kind = VIEW_KIND_FILE;
  std::string html;
};

struct Window {
  Config *configs = nullptr;
  std::map<std::string, View *> views;
  webview_t webview = nullptr;
  std::string current_view = "default";
};

std::expected<Window *, Error> createWindow(Config *config, webview_t wv);
void destroyWindow(Window *window);
void showWindow(Window* window);

void showView(Window *window, std::string name);
void addView(Window *window, std::string name, View *view);

/// Apply native window decorations based on Config (frameless, resizable, etc.).
/// Must be called once after the webview window exists.
void applyWindowStyle(Window *window);

std::expected<View, Error> createView( std::string pathOrCode,
                                      ViewKind kind,
                                      std::optional<ViewConfig> configs);

} // namespace window
} // namespace coconut

#endif // WINDOW_H
