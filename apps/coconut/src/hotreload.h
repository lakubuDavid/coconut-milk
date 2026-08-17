#pragma once

/// @file hotreload.h
///
/// Hot Module Replacement for Lua command files (v0.1.0).
///
/// Provides a manual reload mechanism (no background threads):
///   - coconut.hotreload() → scans commands/ for changed .lua files
///   - Clears package.loaded[module], re-runs .g.lua → ctx:rebind()
///
/// Background-thread auto-watch deferred to v0.2.0.

#include "app.h"
#include "config.h"

namespace coconut::hotreload {

/// (v0.1.0 no-op — reserved for v0.2.0 background watcher.)
void start(App* app, Config* cfg);

/// (v0.1.0 no-op — reserved for v0.2.0 background watcher.)
void stop();

/// Manually trigger a reload scan.
///
/// Compares current last_write_time against cached times for every .lua
/// file in the command root.  Changed files are processed immediately
/// on the calling thread (safe because this is called from Lua on the
/// main thread).
///
/// @param app  The App (needs Lua state + context)
/// @param cfg  The Config (command_root, output_dir, hmr settings)
void trigger(App* app, Config* cfg);

/// Process any pending reloads on the main thread.
///
/// In v0.1.0 this is a no-op because trigger() processes changes
/// synchronously.  Reserved for v0.2.0 background-thread integration.
void drainPending(App* app, Config* cfg);

} // namespace coconut::hotreload
