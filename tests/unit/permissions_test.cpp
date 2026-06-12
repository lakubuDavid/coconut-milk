#include "permissions.h"
#include "test.h"

#include <string>

// ── Permission ↔ String (always pass — API contract) ──────────────────

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

// ── Status ↔ String (always pass — API contract) ──────────────────────

COCONUT_TEST(unit, status_to_string_all) {
  using coconut::permissions::Status;
  using coconut::permissions::toString;

  COCONUT_REQUIRE_EQ(toString(Status::Granted),       std::string("granted"));
  COCONUT_REQUIRE_EQ(toString(Status::Denied),        std::string("denied"));
  COCONUT_REQUIRE_EQ(toString(Status::NotDetermined), std::string("not_determined"));
  COCONUT_REQUIRE_EQ(toString(Status::Restricted),    std::string("restricted"));
  COCONUT_REQUIRE_EQ(toString(Status::NotApplicable), std::string("not_applicable"));
}

// ── Result struct (always pass — API contract) ────────────────────────

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
  COCONUT_REQUIRE_EQ(r.status, Status::Granted);
  COCONUT_REQUIRE(r.message.empty());
}

// ── macOS platform tests ──────────────────────────────────────────────

COCONUT_TEST_MACOS(unit, macos_check_camera_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // Camera permission should be a real status on macOS (not NotApplicable)
  auto r = check(Permission::Camera);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
  // Could be Granted, Denied, NotDetermined, or Restricted
  COCONUT_REQUIRE(r.status == Status::Granted ||
                  r.status == Status::Denied ||
                  r.status == Status::NotDetermined ||
                  r.status == Status::Restricted);
}

COCONUT_TEST_MACOS(unit, macos_check_microphone_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Microphone);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
}

COCONUT_TEST_MACOS(unit, macos_check_notification_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Notification);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
}

COCONUT_TEST_MACOS(unit, macos_check_contacts_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Contacts);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
}

COCONUT_TEST_MACOS(unit, macos_check_photos_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Photos);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
}

COCONUT_TEST_MACOS(unit, macos_check_accessibility_returns_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Accessibility);
  COCONUT_REQUIRE(r.status != Status::NotApplicable);
  // Should be Granted or NotDetermined (no Denied state for Accessibility)
  COCONUT_REQUIRE(r.status == Status::Granted || r.status == Status::NotDetermined);
}

COCONUT_TEST_MACOS(unit, macos_check_admin_returns_not_determined_or_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  auto r = check(Permission::Admin);
  // Most users are not root, so NotDetermined is expected
  COCONUT_REQUIRE(r.status == Status::NotDetermined || r.status == Status::Granted);
}

COCONUT_TEST_MACOS(unit, macos_check_network_always_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // No explicit permission on macOS — always granted
  COCONUT_REQUIRE_EQ(check(Permission::Network).status, Status::Granted);
}

COCONUT_TEST_MACOS(unit, macos_check_clipboard_always_granted) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // No sandbox restriction on desktop macOS
  COCONUT_REQUIRE_EQ(check(Permission::Clipboard).status, Status::Granted);
}

COCONUT_TEST_MACOS(unit, macos_is_available_camera_true) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  COCONUT_REQUIRE(isAvailable(Permission::Camera));
}

COCONUT_TEST_MACOS(unit, macos_is_available_contacts_true) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  // Contacts exists on macOS
  COCONUT_REQUIRE(isAvailable(Permission::Contacts));
}

// ── Cross-platform skip tests ─────────────────────────────────────────

// On Linux, Contacts should not be available
COCONUT_TEST_LINUX(unit, linux_is_available_contacts_false) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  // No Contacts framework on Linux
  COCONUT_REQUIRE(!isAvailable(Permission::Contacts));
}
