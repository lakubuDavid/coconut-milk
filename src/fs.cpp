#include "fs.h"

namespace coconut::fs {

Roots *create(Config *config) {
  return new Roots{.configs = config,
                   .view_root = config != nullptr ? config->view_root : std::string{},
                   .asset_root = config != nullptr ? config->asset_root : std::string{},
                   .command_root = config != nullptr ? config->command_root : std::string{}};
}

void destroy(Roots *roots) {
  delete roots;
}

} // namespace coconut::fs
