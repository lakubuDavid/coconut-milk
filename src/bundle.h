#ifndef COCONUT_BUNDLE_H
#define COCONUT_BUNDLE_H

/// @file bundle.h
///
/// `coconut bundle` — package app into a standalone distributable bundle.
///
/// The bundle pipeline:
///   1. Load the full dev config (debug, manifests, generators all present)
///   2. stripConfig() → produces the **shippable** config (no dev fields)
///   3. Generate platform manifests (Info.plist, app.manifest, .desktop)
///      from the ORIGINAL config (manifests fields needed here)
///   4. Assemble .app directory, copy binary + stripped config + assets
///
/// The stripped config is what ships in the bundle — it contains only
/// runtime fields (window, views, app identity, platform overrides).
/// Dev fields (debug, manifests, generators) are never shipped.

#include "config.h"
#include "error.h"

#include <expected>
#include <string>
#include <vector>

namespace coconut::bundle {

/// Result of one bundle step.
struct StepResult {
  bool ok = false;
  std::string message;
};

/// Run the full bundle pipeline.
///
/// 1. Strips dev fields from cfg → produces shippable config
/// 2. Generates platform manifests from cfg (original, has manifests.*)
/// 3. Assembles bundle directory, copies binary + stripped config + assets
///
/// @param bytecode_config  If true, compile the stripped config to .luac (B2)
StepResult bundle(const Config& cfg,
                  const std::string& out_dir,
                  bool bytecode_config = false);

/// Strip dev-only fields and write the **shippable** config to out_dir.
///
/// The shippable config has:
///   - debug block removed entirely
///   - manifests block removed entirely
///   - platform manifest sub-fields stripped from darwin/win/linux
///
/// Everything else (window, views, app identity, icon paths,
/// platform window overrides, ns permission strings) is preserved.
///
/// Returns the path to the written stripped config file.
std::expected<std::string, Error> writeShippableConfig(
    const Config& cfg,
    const std::string& out_dir);

/// Generate platform manifests (Info.plist, app.manifest, .desktop)
/// from the original cfg (which still has manifests.* fields).
StepResult generateManifests(const Config& cfg,
                             const std::string& out_dir);

/// Assemble bundle directory structure + copy files.
StepResult assembleBundle(const Config& cfg,
                          const std::string& out_dir);

} // namespace coconut::bundle

#endif // COCONUT_BUNDLE_H