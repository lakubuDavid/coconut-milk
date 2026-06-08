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
      std::unordered_map<std::string, sol::protected_function> handlers;
    };

    std::expected<Registry*, Error> create(Config* config);
    void                            destroy(Registry* registry);

  }  // namespace commands
}  // namespace coconut

#endif  // COMMANDS_H
