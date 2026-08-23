#ifndef CORE_DISPATCHER_H
#define CORE_DISPATCHER_H

#include "../app.h"
#include "message_queue.h"
#include "messages.h"

namespace coconut::core {

  struct WorkerPool;  // forward decl — defined in worker.h

  /// Central message hub.
  ///
  /// Receives DispatchMessages (from the Main Runtime / Bridge) and routes
  /// them to the right destination:
  ///   • CommandCallMessage → WorkerPool (background execution)
  ///   • LifecycleMessage   → Main Runtime (Lua: dispatchViewLifecycleEvent)
  ///   • EvalJSMessage      → Webview (webview_eval)
  ///
  /// On every flush() it also drains the WorkerPool's shared Output queue
  /// and routes command results back to the Webview via RPC.
  class Dispatcher {
   private:
    MessageQueue<DispatchMessage> _MessageQueue;
    App*                          _App;
    WorkerPool*                   _WorkerPool;

   public:
    Dispatcher(App* app, WorkerPool* pool);
    void queue(DispatchMessage message);
    void flush();
  };

}  // namespace coconut::core
#endif
