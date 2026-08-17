#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"
#include "error.h"

#include <sol/sol.hpp>

#include <expected>
#include <string>
#include <unordered_map>

namespace coconut {
  namespace commands {

    struct Registry {
      Config*                                                  configs = nullptr;
      /// Commands that run on the background thread (default for ctx:bind).
      std::unordered_map<std::string, sol::protected_function> handlers;
      /// Commands that run on the main thread (registered via ctx:bind_mt).
      std::unordered_map<std::string, sol::protected_function> mt_handlers;
    };

    std::expected<Registry*, Error> create(Config* config);
    void                            destroy(Registry* registry);

  }  // namespace commands
}  // namespace coconut

#endif  // COMMANDS_H
