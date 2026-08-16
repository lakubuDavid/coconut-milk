#ifndef COCONUT_VIEW_EVENTS_H
#define COCONUT_VIEW_EVENTS_H

/// Register NSWindow lifecycle observers (resize, focus, blur).
/// Emits bridge events so the frontend can listen with coconut.on().
/// Must be called after the transport is created.
namespace coconut {
  struct App;
  namespace lifecycle {
    void registerEvents(coconut::App* app);
    void unregisterEvents();
  }
}

#endif // COCONUT_VIEW_EVENTS_H
