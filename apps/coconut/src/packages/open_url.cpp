#include "open_url.h"

#if defined(__APPLE__)
  #include "platform/darwin/open_url.h"
#elif defined(_WIN32)
  #include "platform/win/open_url.h"
#elif defined(__linux__)
  #include "platform/linux/open_url.h"
#endif

// open() is declared extern in packages/open_url.h.
// The platform-specific inline version in open_url.h satisfies the ODR
// across translation units. This TU exists to bring platformOpenUrl into
// the link and to provide a strong symbol for any TU that only includes
// packages/open_url.h (which has extern, not inline).