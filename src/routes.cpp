#include "routes.h"
#include "fs.h"
#include "debug.h"

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace coconut::routes {

static std::set<std::string> g_view_names;

// ── Helpers ─────────────────────────────────────────────────────────

/// Detect MIME type from file extension.
static std::string_view mimeForExtension(const std::string& path) {
  auto dot = path.rfind('.');
  if (dot == std::string::npos) return "application/octet-stream";
  auto ext = path.substr(dot + 1);

  if (ext == "css")   return "text/css";
  if (ext == "js")    return "application/javascript";
  if (ext == "html" || ext == "htm") return "text/html";
  if (ext == "json")  return "application/json";
  if (ext == "png")   return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif")   return "image/gif";
  if (ext == "svg")   return "image/svg+xml";
  if (ext == "ico")   return "image/x-icon";
  if (ext == "woff")  return "font/woff";
  if (ext == "woff2") return "font/woff2";
  if (ext == "ttf")   return "font/ttf";
  if (ext == "map")   return "application/json";
  return "application/octet-stream";
}

/// Strip leading "coconut://" from a URL path.
static std::string stripCoconutScheme(std::string_view path) {
  constexpr std::string_view prefix = "coconut://";
  if (path.size() >= prefix.size() &&
      path.substr(0, prefix.size()) == prefix) {
    path.remove_prefix(prefix.size());
  }
  // Strip leading slash so "home" and "/home" resolve the same way
  if (!path.empty() && path[0] == '/') {
    path.remove_prefix(1);
  }
  return std::string(path);
}

// ── Public API ──────────────────────────────────────────────────────

void setViewNames(const std::set<std::string>& names) {
  g_view_names = names;
  std::string joined;
  for (const auto& n : names) {
    if (!joined.empty()) joined += ", ";
    joined += n;
  }
  debug::info(std::format("routes: {} view(s) registered [{}]", names.size(), joined));
}

RouteResult handle(std::string_view path, std::string_view root_dir) {
  RouteResult result;

  // 1. Normalise the path
  auto normalised = stripCoconutScheme(path);
  if (normalised.empty()) {
    debug::info("routes::handle('') -> NOT_FOUND (empty path)");
    return result;
  }

  // 2. Check if it's a registered view name
  {
    auto it = g_view_names.find(normalised);
    if (it != g_view_names.end()) {
      debug::info(std::format("routes::handle('{}') -> NAVIGATE_VIEW '{}'", normalised, *it));
      result.type = RouteResult::NAVIGATE_VIEW;
      result.view_name = *it;
      return result;
    }
  }

  // 3. Try to serve as a file
  auto filePath = std::filesystem::path(root_dir) / normalised;
  filePath = filePath.lexically_normal();

  auto readResult = fs::readBytes(filePath.string());
  if (!readResult) {
    debug::warn(std::format("routes::handle('{}') -> NOT_FOUND ({})", normalised, filePath.string()));
    return result;  // type stays NOT_FOUND
  }

  debug::info(std::format("routes::handle('{}') -> SERVE_FILE '{}' ({} bytes)",
              normalised, filePath.string(), readResult->size()));

  result.type = RouteResult::SERVE_FILE;
  result.file_path = filePath.string();
  result.mime_type = std::string(mimeForExtension(filePath.string()));
  result.data = std::move(*readResult);
  return result;
}

const std::set<std::string>& getViewNames() {
  return g_view_names;
}

} // namespace coconut::routes
