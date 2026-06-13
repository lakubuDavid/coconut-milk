#include "notify.h"

namespace coconut::notify {

bool platformNotify(const std::string& title, const std::string& body) {
  // TODO: implement with libnotify (notify-send)
  (void)title; (void)body;
  return false;
}

} // namespace coconut::notify