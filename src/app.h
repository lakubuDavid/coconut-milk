#ifndef APP_H
#define APP_H

#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "error.h"
#include "fs.h"
#include "lua_runtime.h"
#include "window.h"

#include <webview/webview.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace coconut {

  /// Top-level runtime owner.
  struct App {
    Config*             configs      = nullptr;
    CoconutContext*     context      = nullptr;

    /// webview handle (created by App core).
    webview_t           webview      = nullptr;

    window::Window*     window       = nullptr;
    lua::Runtime*       lua_state    = nullptr;
    bridge::State*      bridge_state = nullptr;
    commands::Registry* commands     = nullptr;
    fs::Roots*          fs           = nullptr;
    std::vector<Error>  errors;
  };

  namespace app {

    /// Allocate an App. Does not own/destroy the shared Config.
    std::expected<App*, Error> create(Config* configs);

    /// Destroy the App and owned runtime submodules.
    /// Does not destroy the shared Config.
    void destroy(App* app);

    void setConfigs(App* app, Config* cfg);

    std::optional<Error> getError(App* app);
    void                 pushError(App* app, Error err);
    void                 pushError(App* app, ErrorCode code, std::string message, std::string details);

    void run(App* app);

  }  // namespace app

}  // namespace coconut

#endif  // APP_H
