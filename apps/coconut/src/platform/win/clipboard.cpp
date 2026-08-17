#include "clipboard.h"

namespace coconut::clipboard {

std::string platformReadText() {
  // TODO: implement with Win32 clipboard API (OpenClipboard, GetClipboardData)
  return {};
}

bool platformWriteText(const std::string& text) {
  // TODO: implement with Win32 clipboard API
  (void)text;
  return false;
}

} // namespace coconut::clipboard