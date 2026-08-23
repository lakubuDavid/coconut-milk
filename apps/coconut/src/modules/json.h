#ifndef COCONUT_MODULES_JSON_H
#define COCONUT_MODULES_JSON_H

#include "common.h"
#include "thread_kind.h"

#include <sol/sol.hpp>

namespace coconut::modules {

  /// Register coconut.json.jsonify() and coconut.json.parse().
  /// Thread-safe — same implementation on both threads.
  void init_json(sol::state& lua, ThreadKind kind);

  // ── Forward aliases (canonical impl lives in common::) ──────────────
  using common::toJson;
  using common::toTable;

}  // namespace coconut::modules

#endif
