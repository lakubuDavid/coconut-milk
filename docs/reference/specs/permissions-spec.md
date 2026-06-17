# Permissions & Native Notifications

macOS system permission prompts and native notification support.

---

## 1. macOS Permission prompts

### Context

macOS requires `NS*UsageDescription` strings in `Info.plist` for accessing
system resources (camera, microphone, contacts, calendar, etc.). Without these
strings, the app silently fails or crashes when accessing protected resources.

### Configuration

Usage descriptions are set via the `darwin.ns` config block:

```lua
return {
  darwin = {
    ns = {
      usage_descriptions = {
        NSCameraUsageDescription        = "Need camera to scan documents",
        NSMicrophoneUsageDescription    = "Need mic for voice notes",
        NSPhotoLibraryUsageDescription  = "Need photo access for import",
        NSContactsUsageDescription      = "Need contacts for sharing",
        NSCalendarsUsageDescription     = "Need calendar for scheduling",
        NSRemindersUsageDescription     = "Need reminders for task sync",
        NSLocationWhenInUseUsageDescription  = "Need location for tagging",
        NSLocationAlwaysUsageDescription     = "Need location for background tracking",
      },
    },
  },
}
```

### Runtime permission checking

The `permissions` module provides runtime queries:

```cpp
namespace coconut::permissions {

enum class Status { NotDetermined, Granted, Denied, Restricted, Error };

struct PermissionResult {
  Status status;
  std::string message;  ///< Human-readable explanation
};

/// Check and request camera permission (AVFoundation).
PermissionResult checkCamera();

/// Check and request microphone permission.
PermissionResult checkMicrophone();

/// Check and request notification permission (UNUserNotificationCenter).
PermissionResult checkNotifications();

/// Request calendar access (EventKit).
PermissionResult checkCalendar();

/// Request contacts access (Contacts framework).
PermissionResult checkContacts();

/// Request photo library access (Photos framework).
PermissionResult checkPhotos();

} // namespace coconut::permissions
```

Each function triggers the native permission prompt on first call, then returns
the cached status on subsequent calls. Results are exposed to Lua via the bridge.

### Lua API

```lua
local status = coconut.permissions.checkCamera()
-- status = { ok = true/false, message = "..." }
```

---

## 2. Native notifications

### macOS

Coconut supports `NSUserNotification` (pre-macOS 10.14) and
`UNUserNotification` (macOS 10.14+).

### Configuration

```lua
return {
  darwin = {
    ns = {
      notification_alert_style = "alert",  -- "alert" | "banner" | "none"
    },
  },
}
```

- `alert`: Full notification with buttons (requires user permission)
- `banner`: Transient notification (default, no user permission needed in some macOS versions)
- `none`: Silent (no visible notification)

### Lua API

```lua
ctx:notify({
  title   = "Save complete",
  body    = "Your file was saved successfully.",
  icon    = "custom-icon",   -- optional
  actions = {"View", "Dismiss"},  -- optional, UNNotificationAction
})
```

The notification is displayed using the native OS notification system.

### C++ API

```cpp
namespace coconut::notify {

struct Notification {
  std::string title;
  std::string body;
  std::string icon;  ///< Optional: icon name or path
  std::vector<std::string> actions;  ///< Optional: action button labels
};

/// Send a native notification.
void send(const Notification& n);

} // namespace coconut::notify
```

---

## 3. Bundle identifier for notifications

macOS `UNUserNotificationCenter` requires a valid `CFBundleIdentifier` to display
notifications. In development mode (outside a `.app` bundle), the binary may lack
a bundle identifier.

Coconut solves this by embedding an `Info.plist` directly into the `__TEXT,__info_plist`
section of the binary via linker flags:

```
-Wl,-sectcreate,__TEXT,__info_plist,$(projectdir)/res/Info.plist
```

The embedded `Info.plist` contains a hardcoded `CFBundleIdentifier` so that
`NSBundle.mainBundle.bundleIdentifier` returns a valid value even when running
outside a bundled `.app`.

### Resource plist

File: `res/Info.plist`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>com.coconut-milk.runtime</string>
    <key>CFBundleName</key>
    <string>Coconut Milk</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
</dict>
</plist>
```

### Windows / Linux

Notifications on Windows and Linux are not yet implemented. The `notify` module
currently has stub headers only.
