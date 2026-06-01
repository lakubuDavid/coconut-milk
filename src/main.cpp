#include "app.h"
#include "webui.hpp"

using namespace coconut;

int main() {
  // load cfg
  auto cfg =  coconut::Config{.browser = "",
      .window_width                       = 800,
      .window_height                      = 600,
      .initial_view                       = "default",
      .view_root                          = "views/",
      .asset_root                         = "assets/",
      .command_root                       = "commands/"};

  auto app = app::create(&cfg);
  // load lua
  // attach to app
  app::run(app);
  return 0;
}
