/// macOS window creation helpers + bundle detection.
/// Implemented in create_window.mm (ObjC++ file).

#ifndef COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H
#define COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H

#if defined(__APPLE__)

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/// Create a frameless NSWindow.
void* coconut_create_frameless_window(int x, int y, int w, int h);

/// Create a standard NSWindow (with titlebar).
void* coconut_create_standard_window(int x, int y, int w, int h);

/// Detect if running inside a .app bundle and return the Resources path.
/// Returns empty string if not inside a bundle.
const char* coconut_bundle_resource_path();

/// Apply darwin.* config fields to the live NSBundle.
/// Safe to call even if not inside a bundle.
/// @param bundle_identifier  CFBundleIdentifier (or nullptr to skip)
/// @param notification_alert_style  NSUserNotificationAlertStyle value (or nullptr)
/// @param usage_desc_keys   Array of NS*UsageDescription key names (or nullptr)
/// @param usage_desc_values Array of corresponding description strings (or nullptr)
/// @param usage_desc_count Number of entries (0 if arrays are nullptr)
void coconut_apply_darwin_config(const char* bundle_identifier,
                                const char* notification_alert_style,
                                const char** usage_desc_keys,
                                const char** usage_desc_values,
                                int usage_desc_count);

#ifdef __cplusplus
} // extern "C"
#endif

namespace coconut {
namespace platform {

inline void* createFramelessWindow(int x, int y, int w, int h) {
    return coconut_create_frameless_window(x, y, w, h);
}

inline void* createStandardWindow(int x, int y, int w, int h) {
    return coconut_create_standard_window(x, y, w, h);
}

/// Returns the .app/Contents/Resources path if running inside a bundle,
/// or empty string otherwise.
inline std::string detectBundleResourcePath() {
    const char* p = coconut_bundle_resource_path();
    return p ? std::string(p) : std::string();
}

/// Apply darwin.* config to the live NSBundle.
/// Wraps coconut_apply_darwin_config with C++ strings/vectors.
inline void applyDarwinConfig(const std::string& bundle_identifier,
                             const std::string& notification_alert_style,
                             const std::map<std::string, std::string>& usage_descriptions) {
    if (bundle_identifier.empty() && notification_alert_style.empty() && usage_descriptions.empty()) {
        return;  // nothing to apply
    }
    std::vector<const char*> keys;
    std::vector<const char*> vals;
    for (const auto& [k, v] : usage_descriptions) {
        keys.push_back(k.c_str());
        vals.push_back(v.c_str());
    }
    coconut_apply_darwin_config(
        bundle_identifier.c_str(),
        notification_alert_style.c_str(),
        keys.empty() ? nullptr : keys.data(),
        vals.empty() ? nullptr : vals.data(),
        static_cast<int>(keys.size()));
}

} // namespace platform
} // namespace coconut

#endif // __APPLE__

#endif // COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H