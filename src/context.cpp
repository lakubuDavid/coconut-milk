#include "context.h"

namespace coconut {

namespace context {

CoconutContext *create(Config *config) {
  return new CoconutContext{.configs = config,
                            .app = nullptr,
                            .bridge_state = nullptr,
                            .commands = nullptr,
                            .lua_state = nullptr,
                            .window = nullptr};
}

void destroy(CoconutContext *ctx) {
  delete ctx;
}

} // namespace context

CoconutContext *CoconutContext::setBrowser(const std::string &mode) {
  if (configs != nullptr) {
    configs->browser = mode;
  }
  return this;
}

CoconutContext *CoconutContext::setWindowSize(const CoconutWindowSize &size) {
  if (configs != nullptr) {
    configs->window_width = size.w;
    configs->window_height = size.h;
  }
  return this;
}

CoconutContext *CoconutContext::setInitialView(const std::string &name) {
  if (configs != nullptr) {
    configs->initial_view = name;
  }
  return this;
}

void CoconutContext::show(const std::string &name) {
  (void)name;
}

void CoconutContext::reload() {}

void CoconutContext::close() {}

void CoconutContext::bind(const std::string &name, sol::protected_function fn) {
  (void)name;
  (void)fn;
}

void CoconutContext::emit(const std::string &name, sol::object payload) {
  (void)name;
  (void)payload;
}

void CoconutContext::emit_sync(const std::string &name, sol::object payload) {
  (void)name;
  (void)payload;
}

} // namespace coconut
