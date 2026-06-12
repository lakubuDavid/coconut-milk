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
  // First enum value = 0 = Granted
  COCONUT_REQUIRE_EQ(r.status, Status::Granted);
  COCONUT_REQUIRE(r.message.empty());
}

// ── Default implementation — NOT IMPLEMENTED YET ──────────────────────
//
// These tests document expected behavior.  They FAIL until platform-
// specific code (darwin/permissions.mm, win/permissions.cpp, etc.) is
// added.  Red tests prove the feature is not working.

COCONUT_TEST(unit, check_returns_granted_camera) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // TODO: implement macOS AVCaptureDevice.authorizationStatus
  // TODO: implement Windows MediaCapture API
  // TODO: implement Linux /dev/video* check
  COCONUT_REQUIRE_EQ(check(Permission::Camera).status, Status::Granted);
}

COCONUT_TEST(unit, check_returns_granted_admin) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // TODO: implement macOS Authorization Services
  // TODO: implement Windows IsUserAnAdmin
  // TODO: implement Linux geteuid / polkit
  COCONUT_REQUIRE_EQ(check(Permission::Admin).status, Status::Granted);
}

COCONUT_TEST(unit, check_returns_granted_notification) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::check;

  // TODO: implement macOS UNUserNotificationCenter
  // TODO: implement Windows ToastNotifier
  // TODO: implement Linux D-Bus notification daemon
  COCONUT_REQUIRE_EQ(check(Permission::Notification).status, Status::Granted);
}

COCONUT_TEST(unit, request_returns_granted_camera) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::request;

  // TODO: implement platform-specific permission request dialogs
  COCONUT_REQUIRE_EQ(request(Permission::Camera).status, Status::Granted);
}

COCONUT_TEST(unit, request_returns_granted_admin) {
  using coconut::permissions::Permission;
  using coconut::permissions::Status;
  using coconut::permissions::request;

  // TODO: implement macOS AuthorizationCopyRights / Windows UAC / Linux pkexec
  COCONUT_REQUIRE_EQ(request(Permission::Admin).status, Status::Granted);
}

COCONUT_TEST(unit, is_available_returns_true_camera) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  // TODO: Camera exists on macOS, Windows, Linux — platform check should return true
  COCONUT_REQUIRE(isAvailable(Permission::Camera));
}

COCONUT_TEST(unit, is_available_returns_false_contacts_on_linux) {
  using coconut::permissions::Permission;
  using coconut::permissions::isAvailable;

  // TODO: Contacts does not exist on Linux — should return false
  // This test documents expected platform-specific behavior
  // On macOS this should be true; on Linux, false.
  // For now, expect false (not implemented).
  COCONUT_REQUIRE(!isAvailable(Permission::Contacts));
}
