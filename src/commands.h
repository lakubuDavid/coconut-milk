#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"

#include <string>
#include <unordered_map>

namespace coconut {
namespace commands {

struct Registry {
  Config *configs = nullptr;
  std::unordered_map<std::string, int> handlers;
};

Registry *create(Config *config);
void destroy(Registry *registry);

} // namespace commands
} // namespace coconut

#endif // COMMANDS_H
