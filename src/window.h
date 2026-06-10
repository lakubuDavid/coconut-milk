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
  std::string path;   ///< filesystem path (for VIEW_KIND_FILE)
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

/// Set window background color (0-1 range).
void setWindowBackgroundColor(Window* window, float r, float g, float b, float a = 1.0f);

/// Get all registered view names.
std::vector<std::string> getViewNames(Window* window);

std::expected<View, Error> createView( std::string pathOrCode,
                                      ViewKind kind,
                                      std::optional<ViewConfig> configs);

} // namespace window
} // namespace coconut

#endif // WINDOW_H
