#include "new_project.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

// ── Find the script relative to the running binary ───────────────────────────

/// Locate the create-coconut-app script.
/// Search order:
///   1. Next to the running binary: <binary_dir>/../scripts/create-coconut-app
///   2. Next to the binary: <binary_dir>/scripts/create-coconut-app
///   3. COCONUT_SCRIPTS environment variable
/// Returns empty string if not found.
static std::string findScript() {
  // Try to find self path
  std::string selfPath;
#if defined(__APPLE__)
  {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    selfPath.resize(size);
    if (_NSGetExecutablePath(&selfPath[0], &size) == 0) {
      selfPath.resize(size - 1);
    }
  }
#elif defined(__linux__)
  {
    std::error_code ec;
    auto            p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
      selfPath = p.string();
  }
#elif defined(_WIN32)
  {
    std::string buf(MAX_PATH, '\0');
    DWORD       len = GetModuleFileNameA(NULL, &buf[0], MAX_PATH);
    if (len > 0)
      buf.resize(len);
    selfPath = buf;
  }
#endif

  if (selfPath.empty()) {
    // COCONUT_SCRIPTS env fallback
    const char* env = std::getenv("COCONUT_SCRIPTS");
    if (env) {
      auto candidate = std::filesystem::path(env) / "create-coconut-app";
      if (std::filesystem::exists(candidate)) {
        return std::filesystem::absolute(candidate).string();
      }
    }
    return {};
  }

  auto binDir = std::filesystem::path(selfPath).parent_path();

  // Walk up from binary directory looking for scripts/create-coconut-app
  // Handles both installed layouts and deep build dir layouts.
  auto dir = std::filesystem::absolute(binDir);
  for (int i = 0; i < 10; ++i) {
    auto candidate = dir / "scripts" / "create-coconut-app";
    if (std::filesystem::exists(candidate)) {
      return std::filesystem::absolute(candidate).string();
    }
    // Also check scripts/ directly (flat install)
    candidate = dir / "create-coconut-app";
    if (std::filesystem::exists(candidate)) {
      return std::filesystem::absolute(candidate).string();
    }
    auto parent = dir.parent_path();
    if (parent == dir)
      break;  // hit filesystem root
    dir = parent;
  }

  return {};
}

namespace coconut {

  bool scaffoldProject(
      const std::string& name, const std::string& template_name, bool yes, std::string& error_out
  ) {
    // Find the script
    std::string script = findScript();
    if (script.empty()) {
      error_out = "could not find create-coconut-app script";
      return false;
    }

    // Build the command — no --yes flag, interactive by default.
    // Name and template are optional; the script prompts for missing values.
    std::string cmd = script;
    if (!name.empty()) {
      cmd += " " + name;
    }
    if (!template_name.empty() && template_name != "default") {
      cmd += " --template " + template_name;
    }
    if (yes) {
      cmd += " --yes";
    }

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
      error_out = std::format("create-coconut-app exited with code {}", rc);
      return false;
    }

    return true;
  }

}  // namespace coconut
