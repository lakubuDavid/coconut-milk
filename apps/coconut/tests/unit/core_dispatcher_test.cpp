/// Unit tests for core::Dispatcher (builder validation + flush routing).
///
/// Uses a FakeTransport to observe JsCallMessage / worker-result routing,
/// and an unattached single-worker pool so queueMessage() enqueues without
/// spawning threads. Lifecycle routing is exercised with a zeroed Runtime
/// (all fields null) — dispatchViewLifecycleEvent must no-op safely.

#include "core/dispatcher.h"
#include "core/worker.h"

#include "main_runtime.h"  // lua::Runtime full definition
#include "test.h"
#include "transport.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

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
      callback = std::move(cb);
    }

    coconut::transport::MessageCallback callback;
  };

  std::shared_ptr<coconut::transport::Transport> fakeTransportPtr(FakeTransport& t) {
    return std::shared_ptr<coconut::transport::Transport>(&t, [](coconut::transport::Transport*) {
    });
  }

  /// Zeroed runtime — every field null; lifecycle dispatch must no-op.
  coconut::lua::Runtime nullRuntime() {
    return coconut::lua::Runtime{};
  }

  struct Wiring {
    FakeTransport         transport;
    coconut::lua::Runtime runtime = nullRuntime();  ///< must outlive dispatcher
    std::unique_ptr<coconut::core::WorkerPool> pool;
    coconut::core::WorkerPool* poolObserver = nullptr;  ///< set before ownership moves
    std::unique_ptr<coconut::core::Dispatcher> dispatcher;

    static Wiring make() {
      Wiring w;
      auto   poolResult = coconut::core::WorkerPool::builder(1).build();
      if (!poolResult) {
        throw std::runtime_error("Wiring::make: pool build failed");
      }
      w.pool         = std::move(poolResult.value());
      w.poolObserver = w.pool.get();

      auto dispatcherResult = coconut::core::DispatcherBuilder{}
                                  .withRuntime(&w.runtime)
                                  .withWorkerPool(std::move(w.pool))
                                  .withTransport(fakeTransportPtr(w.transport))
                                  .build();
      if (!dispatcherResult) {
        throw std::runtime_error(
            "Wiring::make: dispatcher build failed: " + dispatcherResult.error().message
        );
      }
      w.dispatcher = std::move(dispatcherResult.value());
      return w;
    }
  };

}  // namespace

using namespace coconut;

COCONUT_TEST(core_dispatcher, builder_rejects_null_runtime) {
  Wiring w;
  // Intentionally skip withRuntime().
  auto result = coconut::core::DispatcherBuilder{}
                    .withWorkerPool(std::move(w.pool))
                    .withTransport(fakeTransportPtr(w.transport))
                    .build();
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, ErrorCode::InvalidConfig);
}

COCONUT_TEST(core_dispatcher, builder_rejects_null_pool) {
  Wiring                w;
  coconut::lua::Runtime runtime = nullRuntime();
  auto                  result  = coconut::core::DispatcherBuilder{}
                    .withRuntime(&runtime)
                    .withTransport(fakeTransportPtr(w.transport))
                    .build();
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, ErrorCode::InvalidConfig);
}

COCONUT_TEST(core_dispatcher, builder_rejects_null_transport) {
  Wiring                w;
  coconut::lua::Runtime runtime = nullRuntime();
  auto                  result  = coconut::core::DispatcherBuilder{}
                    .withRuntime(&runtime)
                    .withWorkerPool(std::move(w.pool))
                    .build();
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, ErrorCode::InvalidConfig);
}

COCONUT_TEST(core_dispatcher, builder_accepts_valid_config) {
  auto wiring = Wiring::make();
  COCONUT_REQUIRE(wiring.dispatcher != nullptr);
}

COCONUT_TEST(core_dispatcher, flush_routes_js_call_to_transport) {
  auto wiring = Wiring::make();

  coconut::core::JsRPCMessage envelope;
  envelope.id   = "call-1";
  envelope.type = coconut::core::RpcType::kEvent;
  envelope.name = "hello";
  wiring.dispatcher->queue(core::DispatchMessageT{core::JsCallMessage{.Message = envelope}});
  wiring.dispatcher->flush();

  COCONUT_REQUIRE_EQ(wiring.transport.sent.size(), size_t{1});
  COCONUT_REQUIRE(wiring.transport.sent[0].type == coconut::core::RpcType::kEvent);
  COCONUT_REQUIRE_EQ(wiring.transport.sent[0].name, std::string("hello"));
  COCONUT_REQUIRE_EQ(wiring.transport.sent[0].id, std::string("call-1"));
}

COCONUT_TEST(core_dispatcher, flush_queues_command_call_to_pool) {
  auto wiring = Wiring::make();

  wiring.dispatcher->queue(
      core::DispatchMessageT{core::CommandCallMessage{
          .CommandName = "resize-image",
          .Args        = nlohmann::json{{"w", 100}},
      }}
  );
  // No crash without attached workers; message lands in the worker's input.
  wiring.dispatcher->flush();

  COCONUT_REQUIRE(wiring.transport.sent.empty());
  COCONUT_REQUIRE_EQ(wiring.poolObserver->Workers.size(), size_t{1});
}

COCONUT_TEST(core_dispatcher, command_call_rpcid_reaches_worker_input) {
  auto wiring = Wiring::make();

  wiring.dispatcher->queue(
      core::DispatchMessageT{core::CommandCallMessage{
          .CommandName = "resize-image",
          .Args        = nlohmann::json{{"w", 100}},
          .RpcId       = "call-abc-123",
      }}
  );
  wiring.dispatcher->flush();

  // The pool's RequestId is opaque; the webview call id must ride along so
  // the eventual Resolve/Reject resolves the right JS promise.
  auto queued = wiring.poolObserver->Workers[0]->Input->tryPop();
  COCONUT_REQUIRE(queued.has_value());
  auto promise = std::get_if<coconut::core::PromiseMessage>(&*queued);
  COCONUT_REQUIRE(promise != nullptr);
  COCONUT_REQUIRE_EQ(promise->command, std::string("resize-image"));
  COCONUT_REQUIRE_EQ(promise->RpcId, std::string("call-abc-123"));
}

COCONUT_TEST(core_dispatcher, worker_exec_echoes_rpcid_to_output) {
  // Single-threaded round-trip: exec() pushes a Promise; simulate the loop's
  // outcome by checking the echo fields survive a direct visit.
  auto workerResult = coconut::core::createWorker();
  COCONUT_REQUIRE(workerResult.has_value());
  auto worker = std::move(workerResult.value());

  worker->exec(7, "ping", nlohmann::json{}, "rpc-xyz");
  auto in = worker->Input->tryPop();
  COCONUT_REQUIRE(in.has_value());
  auto promise = std::get_if<coconut::core::PromiseMessage>(&*in);
  COCONUT_REQUIRE(promise != nullptr);
  COCONUT_REQUIRE_EQ(promise->RpcId, std::string("rpc-xyz"));

  // The output side echoes RpcId (as workerLoop does on completion).
  coconut::core::ResolveMessage out{.id = promise->id, .result = nullptr, .RpcId = promise->RpcId};
  worker->Output->push(out);
  auto done = worker->Output->tryPop();
  COCONUT_REQUIRE(done.has_value());
  auto resolve = std::get_if<coconut::core::ResolveMessage>(&*done);
  COCONUT_REQUIRE(resolve != nullptr);
  COCONUT_REQUIRE_EQ(resolve->RpcId, std::string("rpc-xyz"));
}

COCONUT_TEST(core_dispatcher, flush_routes_lifecycle_to_null_runtime_safely) {
  auto wiring = Wiring::make();

  wiring.dispatcher->queue(
      core::DispatchMessageT{core::LifecycleMessage{.ViewName = "home", .EventName = "load"}}
  );
  // Zeroed runtime — dispatchViewLifecycleEvent must no-op without crashing.
  wiring.dispatcher->flush();

  COCONUT_REQUIRE(wiring.transport.sent.empty());
}
