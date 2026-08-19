#include "worker.h"
#include "modules/bg_stubs.h"
#include "modules/bridge_emit.h"
#include "modules/clipboard.h"
#include "modules/dialog.h"
#include "modules/env.h"
#include "modules/fs.h"
#include "modules/hotreload.h"
#include "modules/json.h"
#include "modules/keybind.h"
#include "modules/log.h"
#include "modules/notify.h"
#include "modules/openurl.h"
#include "modules/registry.h"
#include "modules/store.h"
#include "modules/stubs.h"
#include "modules/thread_kind.h"
#include <memory>
#include <optional>
#include <sol/state.hpp>
#include <sol/types.hpp>
#include <thread>

using namespace coconut;
using namespace coconut::modules;

std::expected<std::unique_ptr<coconut::core::Worker>, coconut::Error>
createWorker() {
  auto worker = std::make_unique<coconut::core::Worker>();

  worker.get()->LuaState = std::make_unique<sol::state>();
  worker.get()->LuaState.get()->open_libraries(sol::lib::string, sol::lib::base,
                                               sol::lib::table);

  return worker;
}

std::optional<coconut::Error>
bindModules(coconut::core::Worker *worker,
            coconut::modules::ModulesFlag modules) {
  if (!worker || !worker->LuaState)
    return coconut::Error{.code = coconut::ErrorCode::LuaError,
                          .message = "the worker's lua state not ready"};

  sol::state &lua = *worker->LuaState;

  using F = coconut::modules::ModulesFlag;

  if (has(modules, F::JSON))
    init_json(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::LOG))
    init_log(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::FS))
    init_fs(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::ENV))
    init_env(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::STORE))
    init_store(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::OPENURL))
    init_openurl(lua, ThreadKind::Background);
  if (has(modules, F::KEYBIND))
    init_keybind(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::DIALOG))
    init_dialog(lua, coconut::modules::ThreadKind::Background);
  if (has(modules, F::NOTIFY))
    init_notify(lua, ThreadKind::Background);
  if (has(modules, F::CLIPBOARD))
    init_clipboard(lua, ThreadKind::Background);
  if (has(modules, F::HOTRELOAD))
    init_hotreload(lua, ThreadKind::Background);
  if (has(modules, F::BRIDGE_EMIT))
    init_bridge_emit(lua, ThreadKind::Background);
  if (has(modules, F::STUBS))
    init_stubs(lua, ThreadKind::Background);
  if (has(modules, F::BG_STUBS))
    init_bg_stubs(lua, ThreadKind::Background);

  return std::nullopt;
}
std::optional<coconut::Error> bindCommands(coconut::core::Worker *worker) {
  sol::state &lua = *worker->LuaState;
  for (auto fn : worker->Commands) {
    lua[fn.first] = fn.second;
  }
  return std::nullopt;
}

std::optional<coconut::Error>
coconut::core::attachWorker(coconut::core::Worker *worker) {
  worker->Thread = std::thread([worker]() { workerLoop(worker); });
  return std::nullopt;
}

std::optional<coconut::Error> workerLoop(coconut::core::Worker *worker) {
  return std::nullopt;
}
