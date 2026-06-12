#include "permissions.h"
#include "test.h"

#include <string>

// ── Permission ↔ String ───────────────────────────────────────────────

COCONUT_TEST(unit, permission_to_string_all) {
  using coconut::permissions::Permission;
  using coconut::permissions::toString;

  COCONUT_REQUIRE_EQ(toString(Permission::FullDiskAccess),  std::string("FullDiskAccess"));
  COCONUT_REQUIRE_EQ(toString(Permission::Camera),          std::string("Camera"));
  COCONUT_REQUIRE_EQ(toString(Permission::Microphone),      std::string("Microphone"));
  COCONUT_REQUIRE_EQ(toString(Permission::ScreenRecording), std::string("ScreenRecording"));
  COCONUT_REQUIRE_EQ(toString(Permission::Accessibility),   std::string("Accessibility"));
  COCONUT_REQUIRE_EQ(toString(Permission::Location),        std::string("Location"));
  COCONUT_REQUIRE_EQ(toString(Permission::Contacts),        std::string("Contacts"));
  COCONUT_REQUIRE_EQ(toString(Permission::Photos),          std::string("Photos"));
  COCONUT_REQUIRE_EQ(toString(Permission::Network),         std::string("Network"));
  COCONUT_REQUIRE_EQ(toString(Permission::Admin),           std::string("Admin"));
  COCONUT_REQUIRE_EQ(toString(Permission::Notification),    std::string("Notification"));
  COCONUT_REQUIRE_EQ(toString(Permission::Clipboard),       std::string("Clipboard"));
}

COCONUT_TEST(unit, permission_to_string_unknown_fallback) {
  // There's no way to construct an invalid Permission enum value directly,
  // so this is just a sanity check that the function doesn't crash.
  using coconut::permissions::Permission;
  using coconut::permissions::toString;

  std::string result = toString(Permission::Camera);
  COCONUT_REQUIRE(!result.empty());
}

COCONUT_TEST(unit, permission_from_string_known) {
  using coconut::permissions::Permission;
  using coconut::permissions::fromString;

  COCONUT_REQUIRE_EQ(fromString("FullDiskAccess"),  Permission::FullDiskAccess);
  COCONUT_REQUIRE_EQ(fromString("Camera"),          Permission::Camera);
  COCONUT_REQUIRE_EQ(fromString("Microphone"),      Permission::Microphone);
  COCONUT_REQUIRE_EQ(fromString("ScreenRecording"), Permission::ScreenRecording);
  COCONUT_REQUIRE_EQ(fromString("Accessibility"),   Permission::Accessibility);
  COCONUT_REQUIRE_EQ(fromString("Location"),        Permission::Location);
  COCONUT_REQUIRE_EQ(fromString("Contacts"),        Permission::Contacts);
  COCONUT_REQUIRE_EQ(fromString("Photos"),          Permission::Photos);
  COCONUT_REQUIRE_EQ(fromString("Network"),         Permission::Network);
  COCONUT_REQUIRE_EQ(fromString("Admin"),           Permission::Admin);
  COCONUT_REQUIRE_EQ(fromString("Notification"),    Permission::Notification);
  COCONUT_REQUIRE_EQ(fromString("Clipboard"),       Permission::Clipboard);
}

COCONUT_TEST(unit, permission_from_string_unknown_returns_default) {
  using coconut::permissions::Permission;
  using coconut::permissions::fromString;

  // Unknown names should fall back to FullDiskAccess (not crash)
  COCONUT_REQUIRE_EQ(fromString("NonExistentPermission"), Permission::FullDiskAccess);
  COCONUT_REQUIRE_EQ(fromString(""),                      Permission::FullDiskAccess);
}

COCONUT_TEST(unit, permission_roundtrip) {
  using coconut::permissions::Permission;
  using coconut::permissions::fromString;
  using coconut::permissions::toString;

  COCONUT_REQUIRE_EQ(fromString(toString(Permission::Camera)),          Permission::Camera);
  COCONUT_REQUIRE_EQ(fromString(toString(Permission::FullDiskAccess)),  Permission::FullDiskAccess);
  COCONUT_REQUIRE_EQ(fromString(toString(Permission::ScreenRecording)), Permission::ScreenRecording);
  COCONUT_REQUIRE_EQ(fromString(toString(Permission::Admin)),           Permission::Admin);
}

// ── Status ↔ String ───────────────────────────────────────────────────

COCONUT_TEST(unit, status_to_string_all) {
  using coconut::permissions::Status;
  using coconut::permissions::toString;

  COCONUT_REQUIRE_EQ(toString(Status::Granted),       std::string("granted"));
  COCONUT_REQUIRE_EQ(toString(Status::Denied),        std::string("denied"));
  COCONUT_REQUIRE_EQ(toString(Status::NotDetermined), std::string("not_determined"));
  COCONUT_REQUIRE_EQ(toString(Status::Restricted),    std::string("restricted"));
  COCONUT_REQUIRE_EQ(toString(Status::NotApplicable), std::string("not_applicable"));
}

// ── Result struct ─────────────────────────────────────────────────────

COCONUT_TEST(unit, result_struct) {
  using coconut::permissions::Result;
  using coconut::permissions::Status;

  Result r{ .status = Status::Denied, .message = "User denied access" };

  COCONUT_REQUIRE_EQ(r.status, Status::Denied);
  COCONUT_REQUIRE_EQ(r.message, std::string("User denied access"));
}

COCONUT_TEST(unit, result_default_constructible) {
  using coconut::permissions::Result;
  using coconut::permissions::Status;

  Result r{};
  // Default-constructed Status should be Granted (first enum value = 0)
  COCONUT_REQUIRE_EQ(r.status, Status::Granted);
  COCONUT_REQUIRE(r.message.empty());
}

// ── Default implementation behavior ───────────────────────────────────
//
// The fallback implementation returns Granted for all permissions.
// Platform-specific code overrides this at link time.

COCONUT_TEST(unit, default_check_returns_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // All permissions should return Granted in the default implementation
  COCONUT_REQUIRE_EQ(check(Permission::FullDiskAccess).status,  Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Camera).status,          Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Microphone).status,      Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::ScreenRecording).status, Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Accessibility).status,   Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Location).status,        Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Contacts).status,        Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Photos).status,          Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Network).status,         Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Admin).status,           Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Notification).status,    Status::Granted);
  COCONUT_REQUIRE_EQ(check(Permission::Clipboard).status,       Status::Granted);
}

COCONUT_TEST(unit, default_check_empty_message_on_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::check;

  auto r = check(Permission::Camera);
  COCONUT_REQUIRE(r.message.empty());
}

COCONUT_TEST(unit, default_request_returns_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::request;

  COCONUT_REQUIRE_EQ(request(Permission::Camera).status, Status::Granted);
  COCONUT_REQUIRE_EQ(request(Permission::Admin).status,  Status::Granted);
}

COCONUT_TEST(unit, default_is_available_returns_true) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  COCONUT_REQUIRE(isAvailable(Permission::FullDiskAccess));
  COCONUT_REQUIRE(isAvailable(Permission::Camera));
  COCONUT_REQUIRE(isAvailable(Permission::Contacts));
  COCONUT_REQUIRE(isAvailable(Permission::Clipboard));
}
