#pragma once

/// @file routes.h
///
/// Platform-agnostic route resolver for the coconut:// URL scheme.
///
/// The platform-specific scheme handlers (macOS WKURLSchemeHandler,
/// Windows CoreWebView2, Linux WebKitGTK) catch coconut:// requests
/// and call routes::handle() to get a decision, then translate the
/// RouteResult into the native API calls.  No routing logic lives
/// in the platform adapters.

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace coconut::routes {

// ── RouteResult ─────────────────────────────────────────────────

/// The action a scheme handler should take for a given request.
struct RouteResult {
  enum Type {
    NAVIGATE_VIEW,  ///< Trigger an in-app view navigation
    SERVE_FILE,     ///< Serve a file from disk
    NOT_FOUND,      ///< Respond 404
  };

  Type type = NOT_FOUND;

  // NAVIGATE_VIEW
  std::string view_name;

  // SERVE_FILE
  std::string file_path;   ///< absolute filesystem path (for logging)
  std::string mime_type;   ///< Content-Type
  std::vector<uint8_t> data; ///< file bytes (already read)
};

// ── Public API ─────────────────────────────────────────────────

/// Register the set of known view names.
/// Called from main.cpp after all views are registered.
void setViewNames(const std::set<std::string>& names);

/// Resolve a coconut:// URL and return the action to take.
/// Handles view-name routing, file serving, and 404s.
/// @param url       The full URL (e.g. "assets/app.js" or "home")
/// @param root_dir  Filesystem root for resolving file paths
RouteResult handle(std::string_view path, std::string_view root_dir);

/// Get the full set of registered view names (for debugging).
const std::set<std::string>& getViewNames();

} // namespace coconut::routes
