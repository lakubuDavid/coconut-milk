#include "bundle.h"
#include "config.h"
#include "error.h"
#include "fs.h"

#include <nlohmann/json.hpp>

#include <filesystem>

#include <expected>
#include <format>
#include <fstream>
#include <string>

namespace coconut::bundle {

// ── Write shippable (stripped) config ─────────────────────────────────

std::expected<std::string, Error>
writeShippableConfig(const Config& cfg, const std::string& out_dir) {
  // Step 1: strip dev fields from the original config
  Config shippable = stripConfig(cfg);

  // Step 2: ensure out_dir exists
  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  if (ec) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "failed to create bundle output directory",
      .details = ec.message()
    });
  }

  // Step 3: write the stripped config to out_dir
  // The config is written as JSON for portability (no Lua runtime needed)
  std::string out_path = out_dir + "/coconut.config.json";

  std::ofstream f(out_path);
  if (!f.is_open()) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "failed to write stripped config",
      .details = out_path
    });
  }

  // Serialize the stripped config as JSON
  nlohmann::json j;
  j["window_width"]  = shippable.window_width;
  j["window_height"] = shippable.window_height;
  j["window_min_width"]  = shippable.window_min_width;
  j["window_min_height"] = shippable.window_min_height;
  j["resizable"]   = shippable.resizable;
  j["frameless"]   = shippable.frameless;
  j["transparent"] = shippable.transparent;
  j["title"]       = shippable.title;
  j["initial_view"] = shippable.initial_view;
  j["view_root"]   = shippable.view_root;
  j["asset_root"]  = shippable.asset_root;
  j["command_root"] = shippable.command_root;
  j["output_dir"]  = shippable.output_dir;

  // app identity
  if (!shippable.app.name.empty())        j["app"]["name"]        = shippable.app.name;
  if (!shippable.app.id.empty())          j["app"]["id"]          = shippable.app.id;
  if (!shippable.app.version.empty())     j["app"]["version"]     = shippable.app.version;
  if (!shippable.app.description.empty()) j["app"]["description"] = shippable.app.description;
  if (!shippable.app.category.empty())    j["app"]["category"]    = shippable.app.category;

  // icon paths
  if (!shippable.icon.icns_path.empty()) j["icon"]["icns_path"] = shippable.icon.icns_path;
  if (!shippable.icon.ico_path.empty())  j["icon"]["ico_path"]  = shippable.icon.ico_path;
  if (!shippable.icon.png_path.empty())  j["icon"]["png_path"]  = shippable.icon.png_path;

  // views
  for (const auto& [name, view] : shippable.views) {
    j["views"][name]["kind"] = view.kind;
    j["views"][name]["src"]  = view.src;
  }

  // platform configs (darwin only at runtime, but include all three)
  auto writePlatform = [&](const std::string& key, const PlatformConfig& p) {
    if (p.frameless.has_value())   j[key]["frameless"]   = p.frameless.value();
    if (p.transparent.has_value()) j[key]["transparent"] = p.transparent.value();
    if (!p.app.name.empty())       j[key]["app"]["name"] = p.app.name;
    if (!p.app.id.empty())         j[key]["app"]["id"]   = p.app.id;
    if (!p.bundle_identifier.empty()) j[key]["bundle_identifier"] = p.bundle_identifier;
    // ns permission strings (runtime-relevant)
    if (!p.ns.notification_alert_style.empty())
      j[key]["ns"]["notification_alert_style"] = p.ns.notification_alert_style;
    for (const auto& [k, v] : p.ns.usage_descriptions)
      j[key]["ns"]["usage_descriptions"][k] = v;
  };
  writePlatform("darwin", shippable.darwin);
  writePlatform("win",    shippable.win);
  writePlatform("linux",  shippable.linux);

  f << j.dump(2) << std::endl;
  f.close();

  return out_path;
}

// ── Generate manifests (scaffold) ─────────────────────────────────────

StepResult generateManifests(const Config& cfg, const std::string& out_dir) {
  (void)cfg;
  (void)out_dir;
  return StepResult{
    .ok = false,
    .message = "generateManifests: not yet implemented"
  };
}

// ── Assemble bundle (scaffold) ────────────────────────────────────────

StepResult assembleBundle(const Config& cfg, const std::string& out_dir) {
  (void)cfg;
  (void)out_dir;
  return StepResult{
    .ok = false,
    .message = "assembleBundle: not yet implemented"
  };
}

// ── Full pipeline ─────────────────────────────────────────────────────

StepResult bundle(const Config& cfg,
                  const std::string& out_dir,
                  bool bytecode_config) {
  // Step 1: write shippable config
  auto stripped = writeShippableConfig(cfg, out_dir);
  if (!stripped) {
    return StepResult{
      .ok = false,
      .message = std::format("bundle: failed to write shippable config: {} ({})",
                             stripped.error().message, stripped.error().details)
    };
  }

  // Step 2: generate manifests
  auto manifests = generateManifests(cfg, out_dir);
  if (!manifests.ok) {
    // Not a hard failure — manifests generation is scaffolded
    (void)manifests;
  }

  // Step 3: assemble (scaffolded — non-fatal)
  auto assembled = assembleBundle(cfg, out_dir);
  std::string warnings;
  if (!assembled.ok) {
    warnings = std::format(" (warning: {})", assembled.message);
  }

  return StepResult{
    .ok = true,
    .message = std::format("bundle: shippable config -> {}/coconut.config.json{}",
                           out_dir, warnings)
  };
}

} // namespace coconut::bundle
