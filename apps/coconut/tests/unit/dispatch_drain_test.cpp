/// Tests: dispatch module — now a thin main-loop pump (Phase 2).
///
/// evalJS() routes through the transport's eval(); lifecycleEvent() queues
/// into the core Dispatcher; drain() just flushes the Dispatcher. All tests
/// verify graceful handling of null subsystem pointers.

#include "app.h"
#include "bridge.h"
#include "core/worker.h"
#include "dispatch.h"
#include "main_runtime.h"
#include "modules/registry.h"
#include "test.h"

#include <memory>

namespace {

  /// Captures eval() calls. Never owns a webview.
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

COCONUT_TEST(dispatch, eval_js_routes_through_transport) {
  coconut::App app{};
  auto         transport      = std::make_shared<FakeTransport>();
  app.bridge_state            = new coconut::bridge::State{};
  app.bridge_state->transport = transport;

  coconut::dispatch::evalJS(&app, "console.log('a')");
  coconut::dispatch::evalJS(&app, "console.log('b')");

  COCONUT_REQUIRE_EQ(transport->evaluated.size(), size_t{2});
  COCONUT_REQUIRE_EQ(transport->evaluated[0], std::string("console.log('a')"));
  COCONUT_REQUIRE_EQ(transport->evaluated[1], std::string("console.log('b')"));
}

COCONUT_TEST(dispatch, eval_js_noop_without_transport) {
  coconut::App app{};

  // bridge_state/transport null — must not crash.
  coconut::dispatch::evalJS(&app, "x");
}

COCONUT_TEST(dispatch, lifecycle_noop_without_dispatcher) {
  coconut::App app{};

  // dispatcher null — must not crash and queue nothing.
  coconut::dispatch::lifecycleEvent(&app, "workspace", "load");
}

COCONUT_TEST(dispatch, drain_null_app) {
  // drain(nullptr) must not crash.
  coconut::dispatch::drain(nullptr);
}

COCONUT_TEST(dispatch, drain_without_dispatcher_is_noop) {
  coconut::App app{};

  coconut::dispatch::drain(&app);
  coconut::dispatch::drain(&app);  // twice — still fine
}

COCONUT_TEST(dispatch, drain_flushes_queued_js_calls_via_dispatcher) {
  coconut::App app{};
  auto         transport = std::make_shared<FakeTransport>();

  auto runtimeResult = coconut::lua::create(nullptr, nullptr);
  if (!runtimeResult) {
    return;  // environment without Lua support — skip
  }

  auto poolResult = coconut::core::WorkerPool::builder(1)
                        .withModules(coconut::modules::ModulesFlag::ThreadSafe)
                        .build();
  COCONUT_REQUIRE(poolResult.has_value());

  auto dispatcherResult = coconut::core::DispatcherBuilder{}
                              .withRuntime(runtimeResult.value())
                              .withWorkerPool(std::move(poolResult.value()))
                              .withTransport(transport)
                              .build();
  COCONUT_REQUIRE(dispatcherResult.has_value());
  auto dispatcher = std::move(dispatcherResult.value());

  // Queue an outbound JsCall; drain() must flush it through the transport.
  dispatcher->queue(coconut::core::DispatchMessage{coconut::core::JsCallMessage{
      .Message = coconut::core::JsRPCMessage{
          .type = coconut::core::RpcType::kEvent, .name = "boot", .payload = {}}}});
  COCONUT_REQUIRE(transport->sent.empty());

  app.dispatcher = std::move(dispatcher);
  coconut::dispatch::drain(&app);

  COCONUT_REQUIRE_EQ(transport->sent.size(), size_t{1});
  COCONUT_REQUIRE_EQ(transport->sent[0].name, std::string("boot"));
}
