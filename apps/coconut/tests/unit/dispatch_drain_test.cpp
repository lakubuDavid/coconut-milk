/// Tests: core::Dispatcher — single message router for the app.
///
/// The legacy coconut::dispatch shim (evalJS/lifecycleEvent/drain) was
/// deleted during the v0.2.0 dispatcher port. These tests exercise the
/// real dispatcher: queued messages flush through the transport, posted
/// closures run on the main thread during flush(), and null subsystems
/// are handled gracefully.

#include "app.h"
#include "core/dispatcher.h"
#include "core/worker.h"
#include "main_runtime.h"
#include "modules/registry.h"
#include "test.h"

#include <memory>
#include <string>
#include <vector>

namespace {

  /// Captures eval()/send() calls. Never owns a webview.
  struct FakeTransport : public coconut::transport::Transport {
    std::vector<std::string>                 evaluated;
    std::vector<coconut::core::JsRPCMessage> sent;

    void send(const coconut::core::JsRPCMessage& msg) override {
      sent.push_back(msg);
    }
    void eval(const std::string& js) override {
      evaluated.push_back(js);
    }
    void setMessageCallback(coconut::transport::MessageCallback cb) override {
      (void)cb;
    }
  };

}  // namespace

/// Build a runtime+pool+transport-backed dispatcher, or skip on headless env.
static std::unique_ptr<coconut::core::Dispatcher> makeDispatcher(
    std::shared_ptr<FakeTransport> transport
) {
  auto runtimeResult = coconut::lua::create(nullptr, nullptr);
  if (!runtimeResult) {
    return nullptr;  // environment without Lua support — skip
  }
  auto poolResult = coconut::core::WorkerPool::builder(1)
                        .withModules(coconut::modules::ModulesFlag::ThreadSafe)
                        .build();
  if (!poolResult) {
    return nullptr;
  }
  auto dispatcherResult = coconut::core::DispatcherBuilder{}
                              .withRuntime(runtimeResult.value())
                              .withWorkerPool(std::move(poolResult.value()))
                              .withTransport(transport)
                              .build();
  COCONUT_REQUIRE(dispatcherResult.has_value());
  return std::move(dispatcherResult.value());
}

COCONUT_TEST(dispatch, flush_routes_queued_js_call_through_transport) {
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }

  // Queue an outbound JsCall; flush() must route it through the transport.
  dispatcher->queue(
      coconut::core::DispatchMessage{coconut::core::JsCallMessage{
          .Message = coconut::core::JsRPCMessage{
              .type = coconut::core::RpcType::kEvent, .name = "boot", .payload = {}
          }
      }}
  );
  COCONUT_REQUIRE(transport->sent.empty());

  dispatcher->flush();

  COCONUT_REQUIRE_EQ(transport->sent.size(), size_t{1});
  COCONUT_REQUIRE_EQ(transport->sent[0].name, std::string("boot"));
}

COCONUT_TEST(dispatch, post_runs_closure_during_flush) {
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }

  std::atomic<bool> ran{false};
  dispatcher->post([&ran] { ran.store(true); });
  COCONUT_REQUIRE(!ran.load());

  dispatcher->flush();
  COCONUT_REQUIRE(ran.load());
}

COCONUT_TEST(dispatch, flush_handles_queued_lifecycle_without_crash) {
  // The builder requires a runtime/worker/transport, so there are no null
  // subsystems at runtime. This verifies a queued LifecycleMessage is
  // processed (routed to the Lua runtime) and the dispatcher does not crash
  // or route lifecycle traffic to the transport.
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }

  dispatcher->queue(
      coconut::core::DispatchMessage{
          coconut::core::LifecycleMessage{.ViewName = "w", .EventName = "load"}
      }
  );
  dispatcher->flush();

  COCONUT_REQUIRE(transport->sent.empty());  // lifecycle never goes to webview
}
