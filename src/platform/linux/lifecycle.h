#ifndef COCONUT_PLATFORM_LINUX_LIFECYCLE_H
#define COCONUT_PLATFORM_LINUX_LIFECYCLE_H

namespace coconut {
  struct App;

  namespace lifecycle {
    /// Linux lifecycle stubs — no-op until GTK/X11 window event hooks are implemented.
    void platformRegisterEvents(App* app);
    void platformUnregisterEvents();
  }
}

#endif // COCONUT_PLATFORM_LINUX_LIFECYCLE_H
