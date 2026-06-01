#ifndef FS_H
#define FS_H

#include "config.h"

#include <string>

namespace coconut {
namespace fs {

struct Roots {
  Config *configs = nullptr;
  std::string view_root;
  std::string asset_root;
  std::string command_root;
};

Roots *create(Config *config);
void destroy(Roots *roots);

} // namespace fs
} // namespace coconut

#endif // FS_H
