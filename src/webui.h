#ifndef WEBUI_H
#define WEBUI_H

#include "config.h"

#include <optional>
#include <string>
#include <vector>
#include <webui.hpp>

namespace coconut {
namespace webui {

enum ViewSource {
  VIEW_SOURCE_PATH,
  VIEW_SOURCE_HTML,
  VIEW_SOURCE_URL,
};

struct View {
};

struct ViewConfig {
};

struct Window {
  Config *configs = nullptr;
  std::vector<View> views;
};

Window *create(Config *config);
void destroy(Window *window);

void showView(Window *window, std::string name);
void addView(Window *window, View *view);
View createView(std::string pathOrCode, ViewSource source, std::optional<ViewConfig> configs);

} // namespace webui
} // namespace coconut

#endif // WEBUI_H
