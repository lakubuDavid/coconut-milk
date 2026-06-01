#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace coconut {

  struct Config {
    std::string browser = "auto";
    int         window_width = 1280;
    int         window_height = 640;
    std::string initial_view = "home";
    std::string view_root = "views";
    std::string asset_root = "assets";
    std::string command_root = "commands";
  };

} // namespace coconut

#endif // CONFIG_H
