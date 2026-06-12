#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <string>

namespace coconut::permissions {

// ── Permission Types ──────────────────────────────────────────────────

/// Operating-system permissions that an app may need to request.
///
/// Not all permissions apply to every platform. Use isAvailable() to check.
enum class Permission {
  FullDiskAccess,      ///< Full disk access (macOS FDA / Windows all files / Linux root read)
  Camera,              ///< Camera device access
  Microphone,          ///< Microphone device access
  ScreenRecording,     ///< Screen capture (macOS TCC / Linux portal)
  Accessibility,       ///< Assistive device APIs (macOS)
  Location,            ///< Geolocation services
  Contacts,            ///< Address book / contacts
  Photos,              ///< Photo library access
  Network,             ///< Incoming network (firewall prompt)
  Admin,               ///< Elevated / administrator access (UAC / sudo / Authorization)
  Notification,        ///< Desktop notification permission
  Clipboard,           ///< Clipboard access (sandboxed environments)
};

/// Convert a Permission enum to a string name.
std::string toString(Permission p);

/// Parse a Permission from a string name.
/// Returns Permission::FullDiskAccess on unknown input.
Permission fromString(std::string_view name);

// ── Permission Status ─────────────────────────────────────────────────

/// The result of a permission check or request.
enum class Status {
  Granted,        ///< Permission is available
  Denied,         ///< User denied, cannot request again without OS settings
  NotDetermined,  ///< Not yet requested (prompt will be shown on request)
  Restricted,     ///< Parental controls / MDM / policy restrictions
  NotApplicable,  ///< This permission doesn't apply to this OS
};

/// Convert a Status enum to a string name.
std::string toString(Status s);

// ── Result ────────────────────────────────────────────────────────────

/// Outcome of a permission check or request.
struct Result {
  Status status;
  std::string message;  ///< Human-readable reason (for denied/restricted)
};

// ── Core API ──────────────────────────────────────────────────────────

/// Check the current status of a permission without triggering a prompt.
///
/// This is non-blocking and never shows an OS dialog.
Result check(Permission p);

/// Request a permission from the user.
///
/// This may block to show an OS dialog (UAC prompt, TCC popup, etc.).
/// For permissions with no direct request API (e.g. macOS FullDiskAccess),
/// this opens the relevant system settings pane.
Result request(Permission p);

/// Whether this permission is applicable to the current platform.
///
/// Returns false for permissions that don't exist on this OS
/// (e.g. Contacts on Linux, ScreenRecording on Windows).
bool isAvailable(Permission p);

} // namespace coconut::permissions

#endif // PERMISSIONS_H
