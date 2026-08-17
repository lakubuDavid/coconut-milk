#ifndef COCONUT_OPEN_URL_H
#define COCONUT_OPEN_URL_H

#include <string>

#if defined(__APPLE__)
  #include "platform/darwin/open_url.h"
#elif defined(_WIN32)
  #include "platform/win/open_url.h"
#elif defined(__linux__)
  #include "platform/linux/open_url.h"
#endif

#endif // COCONUT_OPEN_URL_H