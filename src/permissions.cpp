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

// ── Default implementation ────────────────────────────────────────────
//
// This fallback assumes all permissions are granted.  Platform-specific
// implementations (darwin/permissions.mm, win/permissions.cpp,
// linux/permissions.cpp) override these at link time by providing
// stronger symbols with the same names.
//
// The default is intentionally permissive so the module works everywhere
// even before platform code is written.

Result check(Permission /*p*/) {
  return Result{ .status = Status::Granted, .message = "" };
}

Result request(Permission /*p*/) {
  return Result{ .status = Status::Granted, .message = "" };
}

bool isAvailable(Permission /*p*/) {
  return true;
}

} // namespace coconut::permissions
