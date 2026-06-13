#include "clipboard.h"

namespace coconut::clipboard {

std::string platformReadText() {
  // TODO: implement with X11 clipboard
  return {};
}

bool platformWriteText(const std::string& text) {
  // TODO: implement with X11 clipboard
  (void)text;
  return false;
}

} // namespace coconut::clipboard