#include "webui.h"

namespace coconut::webui {

Window *create(Config *config) {
  return new Window{.configs = config, .views = {}};
}

void destroy(Window *window) {
  delete window;
}

void showView(Window *window, std::string name) {
  (void)window;
  (void)name;
}

void addView(Window *window, View *view) {
  (void)window;
  (void)view;
}

View createView(std::string pathOrCode, ViewSource source, std::optional<ViewConfig> configs) {
  (void)pathOrCode;
  (void)source;
  (void)configs;
  return View{};
}

} // namespace coconut::webui
