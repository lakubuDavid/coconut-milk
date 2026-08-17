#include "permissions.h"

#include <string_view>

namespace coconut::permissions {

// ── Permission ↔ String ───────────────────────────────────────────────

static constexpr std::pair<Permission, std::string_view> kPermissionNames[] = {
  { Permission::FullDiskAccess, "FullDiskAccess" },
  { Permission::Camera,         "Camera" },
  { Permission::Microphone,     "Microphone" },
  { Permission::ScreenRecording,"ScreenRecording" },
  { Permission::Accessibility,  "Accessibility" },
  { Permission::Location,       "Location" },
  { Permission::Contacts,       "Contacts" },
  { Permission::Photos,         "Photos" },
  { Permission::Network,        "Network" },
  { Permission::Admin,          "Admin" },
  { Permission::Notification,   "Notification" },
  { Permission::Clipboard,      "Clipboard" },
};

std::string toString(Permission p) {
  for (const auto& [perm, name] : kPermissionNames) {
    if (perm == p) return std::string(name);
  }
  return "Unknown";
}

Permission fromString(std::string_view name) {
  for (const auto& [perm, pname] : kPermissionNames) {
    if (pname == name) return perm;
  }
  return Permission::FullDiskAccess;
}

// ── Status ↔ String ───────────────────────────────────────────────────

std::string toString(Status s) {
  switch (s) {
    case Status::Granted:        return "granted";
    case Status::Denied:         return "denied";
    case Status::NotDetermined:  return "not_determined";
    case Status::Restricted:     return "restricted";
    case Status::NotApplicable:  return "not_applicable";
  }
  return "unknown";
}

// ── Default implementation (weak symbols) ────────────────────────────
//
// Weak symbols allow platform-specific code (darwin/permissions.mm,
// win/permissions.cpp, linux/permissions.cpp) to override these at link
// time by providing strong symbols with the same names.
//
// On platforms without a specific permissions implementation, the weak
// defaults below are used — all returning NotApplicable / false.
//
// Tests that expect Granted will FAIL until platform code is added —
// this is intentional: red tests prove the feature is not implemented.

// MSVC has no weak symbol support; the functions are defined directly.
// On GCC/Clang, weak allows platform-specific overrides.
#if !defined(_MSC_VER)
__attribute__((weak))
#endif
Result check(Permission p) {
  return Result{
    .status = Status::NotApplicable,
    .message = "no platform implementation for " + toString(p),
  };
}

#if !defined(_MSC_VER)
__attribute__((weak))
#endif
Result request(Permission p) {
  return Result{
    .status = Status::NotApplicable,
    .message = "no platform implementation for " + toString(p),
  };
}

#if !defined(_MSC_VER)
__attribute__((weak))
#endif
bool isAvailable(Permission) {
  return false;
}

} // namespace coconut::permissions
