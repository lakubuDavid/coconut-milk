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

} // namespace platform
} // namespace coconut

#endif // __APPLE__

#endif // COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H