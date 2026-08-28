/// Tests: core::Dispatcher integration — runloop ownership + free helpers.
///
/// The legacy coconut::dispatch shim was deleted in the v0.2.0 dispatcher
/// port; init()/shutdown()/flush()/post() now live on core::Dispatcher, and
/// module code reaches the live dispatcher via the free helpers
/// setDispatchApp()/dispatchPost()/dispatchNotify(). These tests cover the
/// current contract and null-safety.

#include "app.h"
#include "core/dispatcher.h"
#include "core/worker.h"
#include "main_runtime.h"
#include "modules/registry.h"
#include "test.h"

#include <atomic>
#include <memory>
#include <string>

namespace {

  /// Captures eval()/send() calls. Never owns a webview.
  struct FakeTransport : public coconut::transport::Transport {
    std::vector<coconut::core::JsRPCMessage> sent;
    void                                     send(const coconut::core::JsRPCMessage& msg) override {
      sent.push_back(msg);
    }
    void eval(const std::string& js) override {
      (void)js;
    }
    void setMessageCallback(coconut::transport::MessageCallback cb) override {
      (void)cb;
    }
  };

  std::unique_ptr<coconut::core::Dispatcher> makeDispatcher(
      std::shared_ptr<FakeTransport> transport
  ) {
    auto runtimeResult = coconut::lua::create(nullptr, nullptr);
    if (!runtimeResult)
      return nullptr;
    auto poolResult = coconut::core::WorkerPool::builder(1)
                          .withModules(coconut::modules::ModulesFlag::ThreadSafe)
                          .build();
    if (!poolResult)
      return nullptr;
    auto dispatcherResult = coconut::core::DispatcherBuilder{}
                                .withRuntime(runtimeResult.value())
                                .withWorkerPool(std::move(poolResult.value()))
                                .withTransport(transport)
                                .build();
    COCONUT_REQUIRE(dispatcherResult.has_value());
    return std::move(dispatcherResult.value());
  }

}  // namespace

// ── init / shutdown ──────────────────────────────────────────────────

COCONUT_TEST(dispatch, init_then_shutdown_is_safe) {
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }
  dispatcher->init();
  dispatcher->flush();
  dispatcher->shutdown();  // shutdown drains before tearing down
}

COCONUT_TEST(dispatch, shutdown_drains_queued_messages) {
  // shutdown() drains remaining messages before tearing down — must not
  // crash and must flush anything queued ahead of it.
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }
  dispatcher->queue(
      coconut::core::DispatchMessageT{coconut::core::JsCallMessage{
          .Message = coconut::core::JsRPCMessage{
              .type = coconut::core::RpcType::kEvent, .name = "bye", .payload = {}
          }
      }}
  );
  dispatcher->shutdown();
  COCONUT_REQUIRE_EQ(transport->sent.size(), size_t{1});
}

// ── free helpers (module-side marshalling) ───────────────────────────

COCONUT_TEST(dispatch, dispatch_post_reaches_live_dispatcher) {
  auto transport  = std::make_shared<FakeTransport>();
  auto dispatcher = makeDispatcher(transport);
  if (!dispatcher) {
    return;  // headless CI — skip
  }

  coconut::core::setDispatchApp(nullptr);  // ensure no stale global
  std::atomic<bool> ran{false};
  coconut::core::dispatchPost([&ran] { ran.store(true); });
  COCONUT_REQUIRE(!ran.load());  // no dispatcher => helper must be a safe no-op

  coconut::App app{};
  app.dispatcher = std::move(dispatcher);
  coconut::core::setDispatchApp(&app);

  coconut::core::dispatchPost([&ran] { ran.store(true); });
  COCONUT_REQUIRE(!ran.load());  // queued, not yet flushed

  app.dispatcher->flush();
  COCONUT_REQUIRE(ran.load());

  coconut::core::setDispatchApp(nullptr);
}

COCONUT_TEST(dispatch, dispatch_notify_safe_without_app) {
  coconut::core::setDispatchApp(nullptr);
  coconut::core::dispatchNotify();  // must not crash before init
}
