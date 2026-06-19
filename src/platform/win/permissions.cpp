/// Win32 permissions implementation.
///
/// Windows does not have the same runtime permission model as macOS.
/// Permissions like camera, microphone, notifications are handled
/// through the Windows Settings app and do not require runtime prompts.
///
/// Overrides the weak default symbols in src/permissions.cpp.

#include "../../permissions.h"

namespace coconut::permissions {

Result check(Permission p) {
  // On Windows, all permissions are available by default.
  // The user manages permissions via Windows Settings > Privacy & security.
  (void)p;
  return Result{
    .status = Status::Granted,
    .message = "granted by default on Windows",
  };
}

Result request(Permission p) {
  // Open Windows Settings > Privacy & security for the relevant category
  // if the user wants to review permissions.
  (void)p;
  return Result{
    .status = Status::Granted,
    .message = "granted by default on Windows",
  };
}

bool isAvailable(Permission p) {
  // Most permissions are available on Windows.
  // ScreenRecording and Accessibility are not applicable in the same way.
  switch (p) {
    case Permission::ScreenRecording:
    case Permission::Accessibility:
    case Permission::FullDiskAccess:
    case Permission::Contacts:
    case Permission::Admin:
      return false; // Not applicable in the same sense as macOS
    default:
      return true;
  }
}

} // namespace coconut::permissions
