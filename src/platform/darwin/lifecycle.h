#ifndef COCONUT_PLATFORM_DARWIN_LIFECYCLE_H
#define COCONUT_PLATFORM_DARWIN_LIFECYCLE_H

namespace coconut {
  struct App;

  namespace lifecycle {
    /// Register NSWindow lifecycle observers (resize, focus, blur)
    /// using the raw Objective-C runtime.
    void platformRegisterEvents(App* app);

    /// Null-out the app pointer so stray callbacks don't fire during teardown.
    void platformUnregisterEvents();
  }
}

#endif // COCONUT_PLATFORM_DARWIN_LIFECYCLE_H
