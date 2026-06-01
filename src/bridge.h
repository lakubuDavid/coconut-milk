#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"

namespace coconut {
namespace bridge {

struct State {
  Config *configs = nullptr;
};

State *create(Config *config);
void destroy(State *state);

} // namespace bridge
} // namespace coconut

#endif // BRIDGE_H
