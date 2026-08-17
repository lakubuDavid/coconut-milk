#ifndef COCONUT_PLATFORM_WIN_LIFECYCLE_H
#define COCONUT_PLATFORM_WIN_LIFECYCLE_H

namespace coconut {
  struct App;

  namespace lifecycle {
    /// Win32 lifecycle stubs — no-op until Windows window event hooks are implemented.
    void platformRegisterEvents(App* app);
    void platformUnregisterEvents();
  }
}

#endif // COCONUT_PLATFORM_WIN_LIFECYCLE_H
