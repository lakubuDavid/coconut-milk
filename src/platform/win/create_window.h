#ifndef COCONUT_PLATFORM_WIN_CREATE_WINDOW_H
#define COCONUT_PLATFORM_WIN_CREATE_WINDOW_H

#if defined(_WIN32)

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/// Create a frameless Win32 window.
void* coconut_create_frameless_window(int x, int y, int w, int h);

/// Create a standard Win32 window (with titlebar).
void* coconut_create_standard_window(int x, int y, int w, int h);

/// Detect the executable directory path.
/// Returns the directory containing the .exe (like Resources dir on macOS).
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

/// Returns the directory containing the executable.
inline std::string detectBundleResourcePath() {
    const char* p = coconut_bundle_resource_path();
    return p ? std::string(p) : std::string();
}

} // namespace platform
} // namespace coconut

#endif // _WIN32

#endif // COCONUT_PLATFORM_WIN_CREATE_WINDOW_H
