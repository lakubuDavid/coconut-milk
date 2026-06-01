#include "bridge.h"

namespace coconut::bridge {

State *create(Config *config) {
  return new State{.configs = config};
}

void destroy(State *state) {
  delete state;
}

} // namespace coconut::bridge
