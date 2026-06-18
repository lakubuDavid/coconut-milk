#include "bundle.h"
#include "config.h"
#include "error.h"
#include "fs.h"
#include "icon_gen.h"

#include <sol/sol.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>

#include <expected>
#include <format>
#include <fstream>
#include <sol/types.hpp>
#include <string>

#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#elif defined(__linux__)
#  include <unistd.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

namespace coconut::bundle {

// ── Helpers ─────────────────────────────────────────────────────────────

/// Find the path to the currently running executable.
/// Returns empty string on failure.
static std::string findSelfPath() {
#if defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(&path[0], &size) == 0) {
    path.resize(size - 1);  // _NSGetExecutablePath includes null in size
    return path;
  }
#elif defined(__linux__)
  std::error_code ec;
  auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (!ec) return p.string();
#elif defined(_WIN32)
  std::string path(MAX_PATH, '\0');
  DWORD len = GetModuleFileNameA(NULL, &path[0], MAX_PATH);
  if (len > 0) {
    path.resize(len);
    return path;
  }
#endif
  return {};
}

/// Copy a single file from src to dst. Creates parent directories.
static bool copyFile(
    const std::string& src,
    const std::string& dst,
    std::string& error_out)
{
  std::error_code ec;
  std::filesystem::create_directories(
      std::filesystem::path(dst).parent_path(), ec);
  if (ec) {
    error_out = std::format("failed to create parent dir for '{}': {}",
                            dst, ec.message());
    return false;
  }
  if (!std::filesystem::copy_file(src, dst,
        std::filesystem::copy_options::overwrite_existing, ec)) {
    error_out = std::format("failed to copy '{}' -> '{}': {}",
                            src, dst, ec.message());
    return false;
  }
  return true;
}

/// Copy an entire directory tree recursively.
static bool copyDir(
    const std::string& src_dir,
    const std::string& dst_dir,
    std::string& error_out)
{
  std::error_code ec;
  std::filesystem::create_directories(dst_dir, ec);
  if (ec) {
    error_out = std::format("failed to create dir '{}': {}", dst_dir, ec.message());
    return false;
  }
  if (!std::filesystem::exists(src_dir, ec)) {
    // Missing source directories are silently skipped (may not exist)
    return true;
  }
  for (auto& entry : std::filesystem::recursive_directory_iterator(src_dir, ec)) {
    if (ec) break;
    const auto& src_path = entry.path();
    auto rel = std::filesystem::relative(src_path, src_dir, ec);
    if (ec) continue;
    auto dst_path = std::filesystem::path(dst_dir) / rel;
    if (entry.is_directory()) {
      std::filesystem::create_directories(dst_path, ec);
    } else {
      std::filesystem::copy_file(src_path, dst_path,
          std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (ec) {
      error_out = std::format("failed to copy '{}': {}", src_path.string(), ec.message());
      return false;
    }
  }
  return true;
}

/// Resolve an app field with platform override.
template <typename F>
static std::string resolveAppField(
    const PlatformConfig& p, const AppConfig& shared, F&& getter)
{
  auto v = getter(p.app);
  if (!v.empty()) return v;
  return getter(shared);
}

/// Resolve bundle identifier: platform.bundle_identifier > platform.app.id > app.id
static std::string resolveBundleId(
    const Config& cfg, const PlatformConfig& p)
{
  if (!p.bundle_identifier.empty()) return p.bundle_identifier;
  if (!p.app.id.empty()) return p.app.id;
  return cfg.app.id;
}

// ── XML helpers ─────────────────────────────────────────────────────────

/// Append an XML plist string value node.
static void plistString(std::ofstream& f, const std::string& key,
                        const std::string& val) {
  f << "  <key>" << key << "</key>\n"
    << "  <string>" << val << "</string>\n";
}

/// Append an XML plist boolean value node.
static void plistBool(std::ofstream& f, const std::string& key, bool val) {
  f << "  <key>" << key << "</key>\n"
    << "  <" << (val ? "true" : "false") << "/>\n";
}

// ── Generate macOS Info.plist ──────────────────────────────────────────

static bool writeInfoPlist(
    const Config& cfg, const std::string& out_dir, std::string& error_out)
{
  const auto& d = cfg.darwin;
  std::string bundle_id  = resolveBundleId(cfg, d);
  std::string app_name   = resolveAppField(d, cfg.app,
      [](const AppConfig& a) { return a.name; });
  std::string app_version = resolveAppField(d, cfg.app,
      [](const AppConfig& a) { return a.version; });
  std::string category   = resolveAppField(d, cfg.app,
      [](const AppConfig& a) { return a.category; });

  std::string path = out_dir + "/Contents/Info.plist";
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());

  std::ofstream f(path);
  if (!f.is_open()) {
    error_out = "failed to write " + path;
    return false;
  }

  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n"
    << "  \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    << "<plist version=\"1.0\">\n"
    << "<dict>\n";

  // Core identity
  plistString(f, "CFBundleIdentifier",      bundle_id);
  plistString(f, "CFBundleName",            app_name);
  plistString(f, "CFBundleDisplayName",     app_name);
  plistString(f, "CFBundleVersion",         app_version);
  plistString(f, "CFBundleShortVersionString", app_version);
  plistString(f, "CFBundlePackageType",     "APPL");
  plistString(f, "CFBundleExecutable",      "coconut");

  // Icon file (matches the generated icon.icns filename)
  plistString(f, "CFBundleIconFile", "icon");

  // Capabilities
  plistBool(f, "NSHighResolutionCapable", true);
  plistString(f, "LSMinimumSystemVersion", "10.13");

  // Category
  if (!category.empty()) {
    // Map some common categories to LSApplicationCategoryType
    std::string lsCat;
    if (category == "developer-tools")
      lsCat = "public.app-category.developer-tools";
    else if (category == "utility")
      lsCat = "public.app-category.utilities";
    else if (category == "productivity")
      lsCat = "public.app-category.productivity";
    else if (category == "education")
      lsCat = "public.app-category.education";
    if (!lsCat.empty())
      plistString(f, "LSApplicationCategoryType", lsCat);
  }

  // Notification style
  if (!d.ns.notification_alert_style.empty())
    plistString(f, "NSUserNotificationAlertStyle", d.ns.notification_alert_style);

  // Usage descriptions
  for (const auto& [key, val] : d.ns.usage_descriptions)
    plistString(f, key, val);

  // Extra Info.plist keys from manifests
  for (const auto& [key, val] : cfg.manifests.darwin_info_plist_extra)
    plistString(f, key, val);

  f << "</dict>\n</plist>\n";
  f.close();
  return true;
}

// ── Generate entitlements ──────────────────────────────────────────────

static bool writeEntitlements(
    const Config& cfg, const std::string& out_dir, std::string& error_out)
{
  if (cfg.manifests.darwin_entitlements.empty()) return true;  // optional

  std::string path = out_dir + "/Contents/entitlements.plist";
  std::ofstream f(path);
  if (!f.is_open()) {
    error_out = "failed to write " + path;
    return false;
  }

  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n"
    << "  \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    << "<plist version=\"1.0\">\n"
    << "<dict>\n";
  for (const auto& [key, val] : cfg.manifests.darwin_entitlements)
    plistString(f, key, val);
  f << "</dict>\n</plist>\n";
  f.close();
  return true;
}

// ── Generate Windows app.manifest ──────────────────────────────────────

static bool writeWindowsManifest(
    const Config& cfg, const std::string& out_dir, std::string& error_out)
{
  const auto& w = cfg.win;
  std::string app_name = resolveAppField(w, cfg.app,
      [](const AppConfig& a) { return a.name; });

  std::string path = out_dir + "/app.manifest";
  std::ofstream f(path);
  if (!f.is_open()) {
    error_out = "failed to write " + path;
    return false;
  }

  f << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
    << "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\"\n"
    << "          manifestVersion=\"1.0\">\n"
    << "  <assemblyIdentity\n"
    << "    name=\"" << app_name << "\"\n"
    << "    version=\"1.0.0.0\"\n"
    << "    type=\"win32\" />\n"
    << "  <description>" << app_name << "</description>\n"
    << "  <dependency>\n"
    << "    <dependentAssembly>\n"
    << "      <assemblyIdentity\n"
    << "        type=\"win32\"\n"
    << "        name=\"Microsoft.Windows.Common-Controls\"\n"
    << "        version=\"6.0.0.0\"\n"
    << "        processorArchitecture=\"*\"\n"
    << "        publicKeyToken=\"6595b64144ccf1df\"\n"
    << "        language=\"*\" />\n"
    << "    </dependentAssembly>\n"
    << "  </dependency>\n"
    << "  <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">\n"
    << "    <application>\n"
    << "      <supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\" />\n"
    << "      <supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\" />\n"
    << "      <supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\" />\n"
    << "      <supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\" />\n"
    << "    </application>\n"
    << "  </compatibility>\n"
    << "</assembly>\n";
  f.close();
  return true;
}

// ── Generate Linux .desktop file ───────────────────────────────────────

static bool writeDesktopFile(
    const Config& cfg, const std::string& out_dir, std::string& error_out)
{
  const auto& l = cfg.linux;
  std::string app_name = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.name; });
  std::string app_desc = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.description; });
  std::string app_id   = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.id; });
  if (app_id.empty()) app_id = "coconut-app";

  std::string path = out_dir + "/" + app_id + ".desktop";
  std::ofstream f(path);
  if (!f.is_open()) {
    error_out = "failed to write " + path;
    return false;
  }

  f << "[Desktop Entry]\n"
    << "Type=Application\n"
    << "Name=" << app_name << "\n"
    << "Comment=" << app_desc << "\n"
    << "Exec=coconut\n"
    << "Terminal=false\n"
    << "Categories=";

  // Map category
  if (!cfg.app.category.empty())
    f << cfg.app.category;
  else
    f << "Utility";
  f << ";\n";

  // Extra desktop keys from manifests
  for (const auto& [key, val] : cfg.manifests.linux_desktop_extra)
    f << key << "=" << val << "\n";

  f.close();
  return true;
}

// ── Generate Linux AppStream metainfo ──────────────────────────────────

static bool writeMetainfo(
    const Config& cfg, const std::string& out_dir, std::string& error_out)
{
  const auto& l = cfg.linux;
  std::string app_name = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.name; });
  std::string app_desc = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.description; });
  std::string app_id   = resolveAppField(l, cfg.app,
      [](const AppConfig& a) { return a.id; });
  if (app_id.empty()) app_id = "coconut-app";

  std::string path = out_dir + "/" + app_id + ".metainfo.xml";
  std::ofstream f(path);
  if (!f.is_open()) {
    error_out = "failed to write " + path;
    return false;
  }

  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    << "<component type=\"desktop-application\">\n"
    << "  <id>" << app_id << "</id>\n"
    << "  <metadata_license>MIT</metadata_license>\n"
    << "  <project_license>MIT</project_license>\n"
    << "  <name>" << app_name << "</name>\n"
    << "  <summary>" << app_desc << "</summary>\n"
    << "  <description>\n"
    << "    <p>" << app_desc << "</p>\n"
    << "  </description>\n";

  if (!cfg.manifests.linux_appstream.empty()) {
    for (const auto& [key, val] : cfg.manifests.linux_appstream)
      f << "  <" << key << ">" << val << "</" << key << ">\n";
  }

  f << "</component>\n";
  f.close();
  return true;
}

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
  if (!shippable.icon.source.empty())    j["icon"]["source"]    = shippable.icon.source;
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

// ── Generate manifests ──────────────────────────────────────────────────

std::expected<std::string, Error> generateManifests(const Config& cfg, const std::string& out_dir) {
  std::string error;

  // macOS — Info.plist
  if (!writeInfoPlist(cfg, out_dir, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "generate manifests: " + error
    });
  }

  // macOS — entitlements (optional)
  if (!writeEntitlements(cfg, out_dir, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "generate manifests: " + error
    });
  }

  // Windows — app.manifest
  if (!writeWindowsManifest(cfg, out_dir, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "generate manifests: " + error
    });
  }

  // Linux — .desktop file
  if (!writeDesktopFile(cfg, out_dir, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "generate manifests: " + error
    });
  }

  // Linux — AppStream metainfo
  if (!writeMetainfo(cfg, out_dir, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "generate manifests: " + error
    });
  }

  return "manifests: Info.plist, app.manifest, .desktop, metainfo.xml generated";
}

// ── Assemble bundle ─────────────────────────────────────────────────────

std::expected<std::string, Error> assembleBundle(const Config& cfg, const std::string& out_dir) {
  std::string error;

  // Detect the running binary path
  std::string self_path = findSelfPath();
  if (self_path.empty()) {
    return std::unexpected(Error{
      .code = ErrorCode::Unknown,
      .message = "assemble: could not find running executable path"
    });
  }

  // macOS: create .app bundle structure
  std::string bundle_root = out_dir + "/Contents";
  std::string macos_dir  = bundle_root + "/MacOS";
  std::string res_dir    = bundle_root + "/Resources";

  std::error_code ec;
  std::filesystem::create_directories(macos_dir, ec);
  if (ec) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = std::format("assemble: failed to create MacOS dir: {}", ec.message())
    });
  }
  std::filesystem::create_directories(res_dir, ec);
  if (ec) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = std::format("assemble: failed to create Resources dir: {}", ec.message())
    });
  }

  // Copy binary to MacOS/coconut
  std::string bin_dst = macos_dir + "/coconut";
  if (!copyFile(self_path, bin_dst, error)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "assemble: " + error
    });
  }
  std::filesystem::permissions(bin_dst,
      std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
      std::filesystem::perms::others_exec | std::filesystem::perms::owner_read |
      std::filesystem::perms::group_read | std::filesystem::perms::others_read,
      std::filesystem::perm_options::add, ec);
  (void)ec;  // non-critical

  // Copy stripped config (written by step 1) into Resources/
  if (std::filesystem::exists(out_dir + "/coconut.config.json")) {
    std::filesystem::rename(out_dir + "/coconut.config.json",
                            res_dir + "/coconut.config.json", ec);
    (void)ec;
  }

  // Copy icon if it exists (explicit path)
  if (!cfg.icon.icns_path.empty() && std::filesystem::exists(cfg.icon.icns_path)) {
    copyFile(cfg.icon.icns_path, res_dir + "/icon.icns", error);
  } else if (std::filesystem::exists(out_dir + "/icon.icns")) {
    // Fallback: icon was auto-generated by icon_gen in out_dir
    copyFile(out_dir + "/icon.icns", res_dir + "/icon.icns", error);
  }

  // Copy project directories into Resources/
  if (!cfg.view_root.empty())
    copyDir(cfg.view_root, res_dir + "/" + cfg.view_root, error);
  if (!cfg.asset_root.empty())
    copyDir(cfg.asset_root, res_dir + "/" + cfg.asset_root, error);
  if (!cfg.command_root.empty())
    copyDir(cfg.command_root, res_dir + "/" + cfg.command_root, error);

  // Copy main.lua if it exists
  if (std::filesystem::exists("main.lua")) {
    copyFile("main.lua", res_dir + "/main.lua", error);
  }

  // Copy generated files (output_dir from config) if the directory exists
  if (!cfg.output_dir.empty() && std::filesystem::exists(cfg.output_dir)) {
    copyDir(cfg.output_dir, res_dir + "/" + cfg.output_dir, error);
  }

  // Copy lib/ if it exists (commonly used for Lua libraries)
  if (std::filesystem::exists("lib")) {
    copyDir("lib", res_dir + "/lib", error);
  }

  return std::format("assemble: .app bundle created at '{}'", out_dir);
}

// ── Bytecode compilation ──────────────────────────────────────────────

std::expected<std::string, Error> compileLuaFile(
    const std::string& lua_path) {
  // Use the embedded LuaJIT (statically linked) to compile to bytecode.
  // This ensures the bytecode format matches exactly what the runtime uses.

  std::string bytecode_path = lua_path + "c";

  try {
    sol::state lua;
    lua.open_libraries(sol::lib::jit,sol::lib::base, sol::lib::string, sol::lib::io, sol::lib::os);

    // Load source file, compile to bytecode via string.dump, write output
    auto script = lua.load(R"(
      local inp, outp = ...
      local f = io.open(inp, "rb")
      if not f then io.stderr:write("cannot open: " .. inp); os.exit(1) end
      local src = f:read("*a")
      f:close()
      local chunk, err = load(src, "@" .. inp)
      if not chunk then io.stderr:write(err); os.exit(1) end
      local bc = string.dump(chunk)
      local o = io.open(outp, "wb")
      o:write(bc)
      o:close()
    )");

    if (!script.valid()) {
      return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "failed to load bytecode compilation script"
      });
    }

    auto result = script(lua_path, bytecode_path);
    if (!result.valid()) {
      sol::error err = result;
      return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "bytecode compilation failed",
        .details = err.what()
      });
    }
  } catch (const std::exception& e) {
    return std::unexpected(Error{
      .code = ErrorCode::LuaError,
      .message = "bytecode compilation exception",
      .details = e.what()
    });
  }

  // Verify the bytecode file was created
  std::error_code ec;
  if (!std::filesystem::exists(bytecode_path, ec)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "bytecode file not created after compilation",
      .details = bytecode_path
    });
  }

  // Replace the original .lua with the compiled bytecode
  std::filesystem::rename(bytecode_path, lua_path, ec);
  if (ec) {
    return bytecode_path;
  }

  return lua_path;
}

std::expected<int, Error> compileLuaDirectory(const std::string& dir) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return 0;  // Directory doesn't exist, nothing to compile
  }

  int compiled = 0;
  for (auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
    if (ec) break;
    if (entry.path().extension() == ".lua") {
      auto result = compileLuaFile(entry.path().string());
      if (result) {
        compiled++;
      } else {
        // Warn but continue compiling other files
        std::println(stderr, "bundle: warning: {}", result.error().message);
      }
    }
  }

  return compiled;
}

// ── Full pipeline ─────────────────────────────────────────────────────

std::expected<std::string, Error> bundle(const Config& cfg,
                                          const std::string& out_dir,
                                          bool bytecode_flag) {
  // Step 1: write shippable config
  auto stripped = writeShippableConfig(cfg, out_dir);
  if (!stripped) {
    return std::unexpected(Error{
      .code = stripped.error().code,
      .message = std::format("bundle: failed to write shippable config: {}",
                             stripped.error().message),
      .details = stripped.error().details
    });
  }

  // Step 2: auto-generate platform icons
  // Uses configured icon.source, or falls back to the embedded default icon.
  std::string app_id = resolveAppField(cfg.darwin, cfg.app,
      [](const AppConfig& a) { return a.id; });
  if (app_id.empty()) app_id = "coconut-app";
  {
    // Only skip icon generation if user explicitly provided all three paths
    bool hasExplicitPaths = !cfg.icon.icns_path.empty() ||
                            !cfg.icon.ico_path.empty()  ||
                            !cfg.icon.png_path.empty();
    if (!hasExplicitPaths) {
      auto icons = icon_gen::generateIcons(cfg.icon.source, out_dir, app_id);
      if (!icons) {
        std::println(stderr, "bundle: warning: icon generation failed: {}",
                     icons.error().message);
      } else {
        std::println("bundle: icons generated (icns={}, ico={}, png={})",
                     icons->icns.empty() ? "none" : icons->icns,
                     icons->ico.empty() ? "none" : icons->ico,
                     icons->png.empty() ? "none" : icons->png);
      }
    }
  }

  // Step 3: generate manifests
  auto manifests = generateManifests(cfg, out_dir);
  if (!manifests) {
    return std::unexpected(Error{
      .code = manifests.error().code,
      .message = std::format("bundle: manifests generation failed: {}",
                             manifests.error().message)
    });
  }

  // Step 4: assemble bundle
  auto assembled = assembleBundle(cfg, out_dir);
  if (!assembled) {
    return std::unexpected(Error{
      .code = assembled.error().code,
      .message = std::format("bundle: assembly failed: {}",
                             assembled.error().message)
    });
  }

  // Step 5 (optional): compile Lua files to bytecode
  if (bytecode_flag) {
    std::string res_dir = out_dir + "/Contents/Resources";
    auto compileResult = compileLuaDirectory(res_dir);
    if (compileResult) {
      std::println("bundle: compiled {} Lua file(s) to bytecode",
                   compileResult.value());
    } else {
      std::println(stderr, "bundle: warning: bytecode compilation failed: {}",
                   compileResult.error().message);
    }
  }

  // Ensure bundle path ends with .app so macOS treats it as a launchable bundle
  std::string bundlePath = out_dir;
  if (!bundlePath.ends_with(".app") && bundlePath.size() >= 4) {
    bundlePath += ".app";
    std::error_code ec;
    // Remove stale bundle if it exists
    if (std::filesystem::exists(bundlePath, ec)) {
      std::filesystem::remove_all(bundlePath, ec);
    }
    std::filesystem::rename(out_dir, bundlePath, ec);
    if (!ec) {
      return std::format("bundle: created -> {}\n  {}", bundlePath, assembled.value());
    }
    // If rename fails, report original path (user can manually add .app)
  }

  return std::format("bundle: created -> {}\n  {}", out_dir, assembled.value());
}

} // namespace coconut::bundle
