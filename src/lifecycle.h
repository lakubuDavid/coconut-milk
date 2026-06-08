#ifndef COCONUT_LIFECYCLE_H
#define COCONUT_LIFECYCLE_H

/// Register NSWindow lifecycle observers (resize, focus, blur).
/// Emits bridge events so the frontend can listen with coconut.on().
/// Must be called after the transport is created.
namespace coconut {
  struct App;
  namespace lifecycle {
    void registerEvents(coconut::App* app);
  }
}

#endif // COCONUT_LIFECYCLE_H
