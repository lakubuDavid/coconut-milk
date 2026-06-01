#ifndef CONTEXT_H
#define CONTEXT_H

#include "config.h"

#include <sol/sol.hpp>
#include <string>

namespace coconut {

  struct App;

  namespace bridge {
    struct State;
  } // namespace bridge

  namespace commands {
    struct Registry;
  } // namespace commands

  namespace lua {
    struct Runtime;
  } // namespace lua

  namespace webui {
    struct Window;
  } // namespace webui

  struct CoconutWindowSize {
    int w = 0;
    int h = 0;
  };

  struct CoconutContext {
    Config*             configs      = nullptr;
    App*                app          = nullptr;
    bridge::State*      bridge_state = nullptr;
    commands::Registry* commands     = nullptr;
    lua::Runtime*       lua_state    = nullptr;
    webui::Window*      window       = nullptr;

    CoconutContext* setBrowser(const std::string& mode);
    CoconutContext* setWindowSize(const CoconutWindowSize& size);
    CoconutContext* setInitialView(const std::string& name);
    void            show(const std::string& name);
    void            reload();
    void            close();
    void            bind(const std::string& name, sol::protected_function fn);
    void            emit(const std::string& name, sol::object payload);
    void            emit_sync(const std::string& name, sol::object payload);
  };

  namespace context {
    CoconutContext* create(Config* config);
    void            destroy(CoconutContext* ctx);
  } // namespace context

} // namespace coconut

#endif // CONTEXT_H
