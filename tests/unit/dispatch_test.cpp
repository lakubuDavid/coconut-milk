/// Tests for the lock-free SPSC dispatch queue (src/dispatch.h / dispatch.cpp).
///
/// Architecture: The Outbox is a single-producer, single-consumer ring buffer.
/// Messages are pushed from any thread and drained on the main thread.
///
/// These tests verify:
///   1. Basic push/pop/empty/size
///   2. FIFO ordering
///   3. Overflow behavior (queue full → push returns false)
///   4. Wrap-around (write index > capacity)
///   5. All three MessageKind values
///   6. Empty drain is a no-op

#include "dispatch.h"
#include "test.h"

#include <string>
#include <thread>
#include <vector>

// ── Fixture ────────────────────────────────────────────────────────────

struct DispatchFixture {
  coconut::dispatch::Outbox outbox;

  /// Push a message into the outbox and return its status.
  bool push(coconut::dispatch::MessageKind kind, const std::string& payload) {
    return outbox.push({kind, payload});
  }

  /// Pop one message, or return a sentinel if empty.
  std::optional<coconut::dispatch::Message> pop() {
    return outbox.pop();
  }

  /// Drain all messages into a vector, returning them in order.
  std::vector<coconut::dispatch::Message> drainAll() {
    std::vector<coconut::dispatch::Message> msgs;
    while (auto m = outbox.pop()) {
      msgs.push_back(std::move(*m));
    }
    return msgs;
  }
};

// ── Basic push / pop / empty / size ───────────────────────────────────

COCONUT_TEST(dispatch, outbox_starts_empty) {
  DispatchFixture f;
  COCONUT_REQUIRE(f.outbox.empty());
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(0));
}

COCONUT_TEST(dispatch, push_increases_size) {
  DispatchFixture f;
  COCONUT_REQUIRE(f.push(coconut::dispatch::MessageKind::EvalJS, "alert(1)"));
  COCONUT_REQUIRE(!f.outbox.empty());
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(1));
}

COCONUT_TEST(dispatch, push_then_pop_restores_empty) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "hello");
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(f.outbox.empty());
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(0));
}

COCONUT_TEST(dispatch, pop_returns_nullopt_when_empty) {
  DispatchFixture f;
  auto msg = f.pop();
  COCONUT_REQUIRE(!msg.has_value());
}

// ── Message fields ────────────────────────────────────────────────────

COCONUT_TEST(dispatch, message_kind_eval_js) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "console.log('hi')");
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::EvalJS);
  COCONUT_REQUIRE_EQ(msg->payload, std::string("console.log('hi')"));
}

COCONUT_TEST(dispatch, message_kind_lifecycle_event) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::LifecycleEvent, "workspace|load");
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::LifecycleEvent);
  COCONUT_REQUIRE_EQ(msg->payload, std::string("workspace|load"));
}

COCONUT_TEST(dispatch, message_kind_command_call) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::CommandCall, R"(set_title|{"t":"hi"})");
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::CommandCall);
  COCONUT_REQUIRE_EQ(msg->payload, std::string(R"(set_title|{"t":"hi"})"));
}

// ── FIFO ordering ─────────────────────────────────────────────────────

COCONUT_TEST(dispatch, fifo_order) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "first");
  f.push(coconut::dispatch::MessageKind::EvalJS, "second");
  f.push(coconut::dispatch::MessageKind::EvalJS, "third");

  auto msgs = f.drainAll();
  COCONUT_REQUIRE_EQ(msgs.size(), size_t(3));
  COCONUT_REQUIRE_EQ(msgs[0].payload, std::string("first"));
  COCONUT_REQUIRE_EQ(msgs[1].payload, std::string("second"));
  COCONUT_REQUIRE_EQ(msgs[2].payload, std::string("third"));
}

COCONUT_TEST(dispatch, interleaved_kinds_preserve_order) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "js1");
  f.push(coconut::dispatch::MessageKind::LifecycleEvent, "load|view");
  f.push(coconut::dispatch::MessageKind::CommandCall, "cmd|{}");
  f.push(coconut::dispatch::MessageKind::EvalJS, "js2");

  auto msgs = f.drainAll();
  COCONUT_REQUIRE_EQ(msgs.size(), size_t(4));
  COCONUT_REQUIRE(msgs[0].kind == coconut::dispatch::MessageKind::EvalJS);
  COCONUT_REQUIRE(msgs[1].kind == coconut::dispatch::MessageKind::LifecycleEvent);
  COCONUT_REQUIRE(msgs[2].kind == coconut::dispatch::MessageKind::CommandCall);
  COCONUT_REQUIRE(msgs[3].kind == coconut::dispatch::MessageKind::EvalJS);
}

// ── Overflow behavior ─────────────────────────────────────────────────

COCONUT_TEST(dispatch, overflow_returns_false) {
  DispatchFixture f;

  // Fill the queue to capacity.
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
    bool ok = f.push(coconut::dispatch::MessageKind::EvalJS, std::to_string(i));
    COCONUT_REQUIRE(ok);
  }

  // Queue is full — next push should fail.
  COCONUT_REQUIRE(!f.push(coconut::dispatch::MessageKind::EvalJS, "overflow"));
  COCONUT_REQUIRE_EQ(f.outbox.size(), coconut::dispatch::kQueueCapacity);
}

COCONUT_TEST(dispatch, overflow_does_not_corrupt_existing_messages) {
  DispatchFixture f;

  // Fill the queue.
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
    f.push(coconut::dispatch::MessageKind::EvalJS, std::to_string(i));
  }

  // Try overflow push — should fail.
  f.push(coconut::dispatch::MessageKind::CommandCall, "should_drop");

  // Drain — should see all original messages, not the overflow.
  auto msgs = f.drainAll();
  COCONUT_REQUIRE_EQ(msgs.size(), coconut::dispatch::kQueueCapacity);
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
    COCONUT_REQUIRE_EQ(msgs[i].payload, std::string(std::to_string(i)));
  }
}

// ── Wrap-around ────────────────────────────────────────────────────────
//
// Push/pop in cycles to force the write index past kQueueCapacity.
// The ring buffer should still behave correctly.

COCONUT_TEST(dispatch, wrap_around_single_element) {
  DispatchFixture f;

  // Fill once.
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
    f.push(coconut::dispatch::MessageKind::EvalJS, std::to_string(i));
  }

  // Drain half.
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity / 2; i++) {
    auto m = f.pop();
    COCONUT_REQUIRE(m.has_value());
    COCONUT_REQUIRE_EQ(m->payload, std::string(std::to_string(i)));
  }

  // Fill more (write index now > kQueueCapacity).
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity / 2; i++) {
    f.push(coconut::dispatch::MessageKind::EvalJS, "wrap_" + std::to_string(i));
  }

  // Drain remaining — should see the rest of the originals then the wraps.
  auto msgs = f.drainAll();
  COCONUT_REQUIRE_EQ(msgs.size(), coconut::dispatch::kQueueCapacity / 2 +
                                   coconut::dispatch::kQueueCapacity / 2);

  // First half: remaining originals (from kQueueCapacity/2 to end).
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity / 2; i++) {
    COCONUT_REQUIRE_EQ(msgs[i].payload,
                       std::string(std::to_string(i + coconut::dispatch::kQueueCapacity / 2)));
  }

  // Second half: the wrapped-around new pushes.
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity / 2; i++) {
    COCONUT_REQUIRE_EQ(msgs[i + coconut::dispatch::kQueueCapacity / 2].payload,
                       std::string("wrap_" + std::to_string(i)));
  }
}

COCONUT_TEST(dispatch, wrap_around_full_cycle) {
  DispatchFixture f;

  // Fill → drain → fill → drain: 3 full cycles.
  for (size_t cycle = 0; cycle < 3; cycle++) {
    for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
      f.push(coconut::dispatch::MessageKind::EvalJS,
             std::to_string(cycle) + ":" + std::to_string(i));
    }
    auto msgs = f.drainAll();
    COCONUT_REQUIRE_EQ(msgs.size(), coconut::dispatch::kQueueCapacity);
    for (size_t i = 0; i < coconut::dispatch::kQueueCapacity; i++) {
      COCONUT_REQUIRE_EQ(msgs[i].payload,
                         std::to_string(cycle) + ":" + std::to_string(i));
    }
  }
}

// ── Drain when empty ──────────────────────────────────────────────────

COCONUT_TEST(dispatch, drain_empty_is_noop) {
  DispatchFixture f;
  auto msgs = f.drainAll();
  COCONUT_REQUIRE(msgs.empty());
}

COCONUT_TEST(dispatch, drain_after_partial_pop) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "a");
  f.push(coconut::dispatch::MessageKind::EvalJS, "b");
  f.push(coconut::dispatch::MessageKind::EvalJS, "c");

  // Pop one.
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE_EQ(msg->payload, std::string("a"));

  // Drain the rest.
  auto msgs = f.drainAll();
  COCONUT_REQUIRE_EQ(msgs.size(), size_t(2));
  COCONUT_REQUIRE_EQ(msgs[0].payload, std::string("b"));
  COCONUT_REQUIRE_EQ(msgs[1].payload, std::string("c"));
}

// ── Message payload with special characters ───────────────────────────

COCONUT_TEST(dispatch, payload_with_pipe) {
  DispatchFixture f;
  // Pipe is the delimiter in LifecycleEvent and CommandCall payloads,
  // but the Outbox is payload-agnostic — it stores whatever string.
  f.push(coconut::dispatch::MessageKind::LifecycleEvent,
         "my_view|load|extra|info");
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE_EQ(msg->payload, std::string("my_view|load|extra|info"));
}

COCONUT_TEST(dispatch, payload_with_json_special_chars) {
  DispatchFixture f;
  std::string json = R"({"path":"/Users/me/file.txt","size":1024})";
  f.push(coconut::dispatch::MessageKind::CommandCall, "open|" + json);
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE_EQ(msg->payload, std::string("open|") + json);
}

// ── Empty after drain ─────────────────────────────────────────────────

COCONUT_TEST(dispatch, empty_after_drain) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "x");
  f.push(coconut::dispatch::MessageKind::EvalJS, "y");
  f.drainAll();
  COCONUT_REQUIRE(f.outbox.empty());
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(0));
}

COCONUT_TEST(dispatch, pop_after_drain_returns_nullopt) {
  DispatchFixture f;
  f.push(coconut::dispatch::MessageKind::EvalJS, "x");
  f.drainAll();
  auto msg = f.pop();
  COCONUT_REQUIRE(!msg.has_value());
}

// ── Edge cases ────────────────────────────────────────────────────────

COCONUT_TEST(dispatch, push_empty_payload) {
  DispatchFixture f;
  COCONUT_REQUIRE(f.push(coconut::dispatch::MessageKind::EvalJS, ""));
  auto msg = f.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->payload.empty());
}

COCONUT_TEST(dispatch, size_after_mixed_push_pop) {
  DispatchFixture f;

  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(0));
  f.push(coconut::dispatch::MessageKind::EvalJS, "a");
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(1));
  f.push(coconut::dispatch::MessageKind::EvalJS, "b");
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(2));
  f.pop();
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(1));
  f.pop();
  COCONUT_REQUIRE_EQ(f.outbox.size(), size_t(0));
}
