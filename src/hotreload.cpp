#include "hotreload.h"
#include "debug.h"
#include "generators/generate.h"  // for coconut::generator::runGenerate

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <string>
#include <vector>

namespace coconut::hotreload {

namespace {

// ── File tracking (no thread — used by manual trigger()) ─────────────

/// Map from filename → last write time.
std::map<std::string, std::filesystem::file_time_type> g_last_times;

/// Seed the tracking map from a directory.
void seedTimes(const std::filesystem::path& dir) {
  if (!std::filesystem::is_directory(dir)) return;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".lua") continue;
    // Skip .g.lua files (generated) — we watch the source .lua files.
    auto stem = entry.path().stem().string();
    if (stem.size() > 2 && stem.substr(stem.size() - 2) == ".g") continue;
    std::error_code ec;
    g_last_times[entry.path().filename().string()] = std::filesystem::last_write_time(entry.path(), ec);
  }
}

/// Collect files that have changed since the last check (or new files).
/// @returns sorted list of absolute paths to changed files.
std::vector<std::string> collectChanges(const std::filesystem::path& dir) {
  std::vector<std::string> changed;
  std::error_code ec;

  if (!std::filesystem::is_directory(dir, ec)) return changed;

  // Check existing files
  for (auto& [name, last] : g_last_times) {
    auto path = dir / name;
    if (!std::filesystem::exists(path, ec)) continue;
    auto current = std::filesystem::last_write_time(path, ec);
    if (ec) continue;
    if (current != last) {
      last = current;
      changed.push_back(path.string());
    }
  }

  // Check for new files
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (entry.path().extension() != ".lua") continue;
    auto stem = entry.path().stem().string();
    if (stem.size() > 2 && stem.substr(stem.size() - 2) == ".g") continue;
    auto name = entry.path().filename().string();
    if (g_last_times.find(name) == g_last_times.end()) {
      std::error_code ec2;
      g_last_times[name] = std::filesystem::last_write_time(entry.path(), ec2);
      changed.push_back(entry.path().string());
    }
  }

  return changed;
}

// ── Reload processing (runs on main thread) ──────────────────────────

/// Reload a single changed command module.
/// @param changed_path  Absolute path to the changed .lua file
void reloadModule(const std::string& changed_path,
                  coconut::App* app,
                  const std::string& command_root,
                  const std::string& generated_dir,
                  bool auto_gen) {
  if (!app || !app->lua_state || !app->lua_state->lua_state || !app->context) {
    debug::warn("hotreload: app not fully initialized, skipping reload");
    return;
  }

  // Extract module name from path.
  // e.g. "commands/example.lua" → "example"
  // e.g. "commands/subdir/module.lua" → "subdir.module"
  std::filesystem::path path(changed_path);
  auto rel = std::filesystem::relative(path, command_root).replace_extension("");
  std::string module_name = rel.string();
  // Normalize path separators to Lua module format
  for (auto& c : module_name) {
    if (c == '/' || c == '\\') c = '.';
  }

  debug::info(std::format("hotreload: reloading module '{}'", module_name));

  // Step 1: Optionally regenerate .g.lua files
  if (auto_gen) {
    debug::info("hotreload: auto-regenerating...");
    coconut::generator::runGenerate(command_root, generated_dir);
  }

  // Step 2: Clear Lua module cache
  sol::state_view lua(*app->lua_state->lua_state);
  auto clear_result = lua.safe_script(
      "package.loaded[\"" + module_name + "\"] = nil",
      sol::script_pass_on_error);
  if (!clear_result.valid()) {
    sol::error e = clear_result;
    debug::warn(std::format("hotreload: failed to clear package.loaded['{}']: {}",
                            module_name, e.what()));
  }

  // Step 3: Find and re-run the .g.lua wrapper
  // Look in generated_dir, then command_root
  std::string g_lua_name = module_name;
  for (auto& c : g_lua_name) {
    if (c == '.') c = '/';
  }
  g_lua_name += ".g.lua";

  std::filesystem::path g_path;
  auto candidate1 = std::filesystem::path(generated_dir) / g_lua_name;
  auto candidate2 = std::filesystem::path(command_root) / g_lua_name;

  if (std::filesystem::exists(candidate1)) {
    g_path = candidate1;
  } else if (std::filesystem::exists(candidate2)) {
    g_path = candidate2;
  } else {
    debug::warn(std::format("hotreload: no .g.lua found for '{}' (checked {} and {})",
                            module_name, candidate1.string(), candidate2.string()));
    return;
  }

  debug::info(std::format("hotreload: re-running '{}'", g_path.string()));

  // Load and run the .g.lua file
  auto loadResult = lua.safe_script_file(g_path.string(), sol::script_pass_on_error);
  if (!loadResult.valid()) {
    sol::error e = loadResult;
    debug::warn(std::format("hotreload: failed to load '{}': {}",
                            g_path.filename().string(), e.what()));
    return;
  }

  // The file should return a register(ctx) function
  sol::object ret = loadResult;
  if (!ret.is<sol::function>()) {
    debug::warn(std::format("hotreload: '{}' did not return a function (returned type {})",
                            g_path.filename().string(),
                            static_cast<int>(ret.get_type())));
    return;
  }

  // Call register(ctx)
  sol::object ctx_obj = lua["ctx"];
  if (!ctx_obj.valid()) {
    debug::warn("hotreload: ctx not available in Lua state");
    return;
  }

  auto bindResult = ret.as<sol::function>()(ctx_obj);
  if (!bindResult.valid()) {
    sol::error e = bindResult;
    debug::warn(std::format("hotreload: register({}) failed: {}", module_name, e.what()));
    return;
  }

  debug::info(std::format("hotreload: '{}' reloaded successfully", module_name));
}

}  // anonymous namespace

// ── Public API (v0.1.0 — no background threads) ─────────────────────

void start(App* /*app*/, Config* /*cfg*/) {
  // v0.1.0 no-op.  Background watcher deferred to v0.2.0.
  debug::info("hotreload: background watcher not available in v0.1.0 (use coconut.hotreload() manually)");
}

void stop() {
  // v0.1.0 no-op.  Nothing to stop.
}

void trigger(App* app, Config* cfg) {
  if (!app || !cfg) return;

  // Seed file tracking on first call
  if (g_last_times.empty()) {
    seedTimes(cfg->command_root);
  }

  // Scan for changes
  auto changed = collectChanges(cfg->command_root);
  if (changed.empty()) {
    debug::info("hotreload: no changes detected");
    return;
  }

  debug::info(std::format("hotreload: {} file(s) changed, reloading...", changed.size()));

  // Process each changed file immediately (main thread — called from Lua)
  for (const auto& path : changed) {
    reloadModule(path, app, cfg->command_root, cfg->output_dir, cfg->hmr.auto_regenerate);
  }
}

void drainPending(App* /*app*/, Config* /*cfg*/) {
  // v0.1.0 no-op.  trigger() processes changes synchronously.
  // In v0.2.0 this will drain the background thread's file-change queue.
}

}  // namespace coconut::hotreload
