#include "commands.h"

namespace coconut::commands {

std::expected<Registry*, Error> create(Config* config) {
  return new Registry{.configs = config, .handlers = {}};
}

void destroy(Registry* registry) {
  delete registry;
}

}  // namespace coconut::commands
