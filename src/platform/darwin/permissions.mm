#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <UserNotifications/UserNotifications.h>
#import <Contacts/Contacts.h>
#import <Photos/Photos.h>
#import <Security/Authorization.h>
#import <ApplicationServices/ApplicationServices.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "permissions.h"

#include <unistd.h>

namespace coconut::permissions {

// ── Helpers ────────────────────────────────────────────────────────────

static Status toAVStatus(AVAuthorizationStatus s) {
  switch (s) {
    case AVAuthorizationStatusAuthorized:    return Status::Granted;
    case AVAuthorizationStatusDenied:        return Status::Denied;
    case AVAuthorizationStatusRestricted:    return Status::Restricted;
    case AVAuthorizationStatusNotDetermined: return Status::NotDetermined;
  }
  return Status::NotDetermined;
}

static Status toUNStatus(UNAuthorizationStatus s) {
  switch (s) {
    case UNAuthorizationStatusAuthorized:    return Status::Granted;
    case UNAuthorizationStatusDenied:        return Status::Denied;
    case UNAuthorizationStatusNotDetermined: return Status::NotDetermined;
    case UNAuthorizationStatusProvisional:   return Status::Granted;
  }
  return Status::NotDetermined;
}

static Status toCNStatus(CNAuthorizationStatus s) {
  switch (s) {
    case CNAuthorizationStatusAuthorized:    return Status::Granted;
    case CNAuthorizationStatusDenied:        return Status::Denied;
    case CNAuthorizationStatusRestricted:    return Status::Restricted;
    case CNAuthorizationStatusNotDetermined: return Status::NotDetermined;
  }
  return Status::NotDetermined;
}

static Status toPHStatus(PHAuthorizationStatus s) {
  switch (s) {
    case PHAuthorizationStatusAuthorized:    return Status::Granted;
    case PHAuthorizationStatusDenied:        return Status::Denied;
    case PHAuthorizationStatusRestricted:    return Status::Restricted;
    case PHAuthorizationStatusNotDetermined: return Status::NotDetermined;
    case PHAuthorizationStatusLimited:       return Status::Granted;
  }
  return Status::NotDetermined;
}

// ── Request helpers (blocking via semaphore) ───────────────────────────

static Status requestAVMedia(AVMediaType type) {
  __block AVAuthorizationStatus result = AVAuthorizationStatusNotDetermined;
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);

  [AVCaptureDevice requestAccessForMediaType:type
                           completionHandler:^(BOOL granted) {
    result = granted ? AVAuthorizationStatusAuthorized
                     : AVAuthorizationStatusDenied;
    dispatch_semaphore_signal(sem);
  }];

  dispatch_semaphore_wait(sem,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)));
  return toAVStatus(result);
}

static Status requestNotification() {
  __block UNAuthorizationStatus result = UNAuthorizationStatusNotDetermined;
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);

  UNUserNotificationCenter* nc = [UNUserNotificationCenter currentNotificationCenter];
  [nc requestAuthorizationWithOptions:(UNAuthorizationOptionAlert |
                                       UNAuthorizationOptionSound |
                                       UNAuthorizationOptionBadge)
                    completionHandler:^(BOOL granted, NSError* err) {
    (void)err;
    result = granted ? UNAuthorizationStatusAuthorized
                     : UNAuthorizationStatusDenied;
    dispatch_semaphore_signal(sem);
  }];

  dispatch_semaphore_wait(sem,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)));
  return toUNStatus(result);
}

static Status requestContacts() {
  __block CNAuthorizationStatus result = CNAuthorizationStatusNotDetermined;
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);

  CNContactStore* store = [[CNContactStore alloc] init];
  [store requestAccessForEntityType:CNEntityTypeContacts
                  completionHandler:^(BOOL granted, NSError* err) {
    (void)err;
    result = granted ? CNAuthorizationStatusAuthorized
                     : CNAuthorizationStatusDenied;
    dispatch_semaphore_signal(sem);
  }];

  dispatch_semaphore_wait(sem,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)));
  return toCNStatus(result);
}

static Status requestPhotos() {
  __block PHAuthorizationStatus result = PHAuthorizationStatusNotDetermined;
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);

  [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
    result = status;
    dispatch_semaphore_signal(sem);
  }];

  dispatch_semaphore_wait(sem,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)));
  return toPHStatus(result);
}

// ── Check: Permission → Status ─────────────────────────────────────────

static Status checkPlatform(Permission p) {
  switch (p) {
    case Permission::Camera:
      return toAVStatus(
          [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]);

    case Permission::Microphone:
      return toAVStatus(
          [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]);

    case Permission::Notification:
      // No blocking check on macOS — use known status or NotDetermined
      return Status::NotDetermined;

    case Permission::Contacts:
      return toCNStatus(
          [CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts]);

    case Permission::Photos:
      return toPHStatus([PHPhotoLibrary authorizationStatus]);

    case Permission::Accessibility:
      return AXIsProcessTrusted() ? Status::Granted : Status::NotDetermined;

    case Permission::Admin:
      return (geteuid() == 0) ? Status::Granted : Status::NotDetermined;

    case Permission::ScreenRecording: {
      // Use ScreenCaptureKit to test permission
      dispatch_semaphore_t sem = dispatch_semaphore_create(0);
      __block bool hasPermission = false;
      [SCShareableContent getShareableContentWithCompletionHandler:^(
          SCShareableContent* content, NSError* error) {
        hasPermission = (content != nil && error == nil);
        dispatch_semaphore_signal(sem);
      }];
      dispatch_semaphore_wait(sem,
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)));
      return hasPermission ? Status::Granted : Status::NotDetermined;
    }

    case Permission::FullDiskAccess: {
      NSString* path =
          @"/Library/Preferences/SystemConfiguration/"
           "com.apple.airport.preferences.plist";
      BOOL readable = [[NSFileManager defaultManager] isReadableFileAtPath:path];
      return readable ? Status::Granted : Status::NotDetermined;
    }

    case Permission::Location:
      return Status::NotDetermined;

    case Permission::Network:
      return Status::Granted;

    case Permission::Clipboard:
      return Status::Granted;
  }
}

// ── Request: Permission → Status ───────────────────────────────────────

static Status requestPlatform(Permission p) {
  switch (p) {
    case Permission::Camera:
      return requestAVMedia(AVMediaTypeVideo);

    case Permission::Microphone:
      return requestAVMedia(AVMediaTypeAudio);

    case Permission::Notification:
      return requestNotification();

    case Permission::Contacts:
      return requestContacts();

    case Permission::Photos:
      return requestPhotos();

    case Permission::Accessibility: {
      NSDictionary* opts = @{
        (__bridge id)kAXTrustedCheckOptionPrompt: @YES
      };
      BOOL trusted = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
      return trusted ? Status::Granted : Status::Denied;
    }

    case Permission::Admin: {
      AuthorizationRef authRef = nullptr;
      OSStatus status = AuthorizationCreate(
          nullptr, kAuthorizationEmptyEnvironment,
          kAuthorizationFlagDefaults, &authRef);
      if (status == errAuthorizationSuccess && authRef) {
        AuthorizationItem items[] = {
          { kAuthorizationRightExecute, 0, nullptr, 0 }
        };
        AuthorizationRights rights = { 1, items };
        status = AuthorizationCopyRights(
            authRef, &rights, nullptr,
            kAuthorizationFlagDefaults |
            kAuthorizationFlagInteractionAllowed |
            kAuthorizationFlagPreAuthorize |
            kAuthorizationFlagExtendRights,
            nullptr);
        AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
        return (status == errAuthorizationSuccess)
            ? Status::Granted : Status::Denied;
      }
      return Status::Denied;
    }

    case Permission::ScreenRecording:
    case Permission::FullDiskAccess:
      [[NSWorkspace sharedWorkspace] openURL:
          [NSURL URLWithString:
              @"x-apple.systempreferences:"
              @"com.apple.preference.security"
              @"?Privacy_ScreenCapture"]];
      return Status::NotDetermined;

    case Permission::Location:
    case Permission::Network:
    case Permission::Clipboard:
      return Status::Granted;
  }
}

// ── Public API ─────────────────────────────────────────────────────────

Result check(Permission p) {
  Status s = checkPlatform(p);
  std::string msg;
  if (s == Status::Denied) {
    msg = toString(p) + " not granted — open System Settings → Privacy & Security";
  } else if (s == Status::Restricted) {
    msg = toString(p) + " restricted by parental controls or MDM";
  }
  return Result{ .status = s, .message = msg };
}

Result request(Permission p) {
  Status s = requestPlatform(p);
  std::string msg;
  if (s == Status::Denied) {
    msg = toString(p) + " not granted — open System Settings → Privacy & Security";
  } else if (s == Status::Restricted) {
    msg = toString(p) + " restricted by parental controls or MDM";
  }
  return Result{ .status = s, .message = msg };
}

bool isAvailable(Permission p) {
  switch (p) {
    case Permission::Camera:
    case Permission::Microphone:
    case Permission::Notification:
    case Permission::Contacts:
    case Permission::Photos:
    case Permission::Accessibility:
    case Permission::ScreenRecording:
    case Permission::FullDiskAccess:
    case Permission::Admin:
    case Permission::Network:
    case Permission::Clipboard:
      return true;
    case Permission::Location:
      return false;
  }
  return false;
}

} // namespace coconut::permissions
