/// macOS window creation helpers.
/// These are implemented in create_window.mm (ObjC++ file).

#ifndef COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H
#define COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H

#if defined(__APPLE__)

#ifdef __cplusplus
extern "C" {
#endif

/// Create a frameless NSWindow.
/// Returns a void* (the NSWindow*) that can be passed to webview_create().
void* coconut_create_frameless_window(int x, int y, int w, int h);

/// Create a standard NSWindow (with titlebar).
void* coconut_create_standard_window(int x, int y, int w, int h);

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

} // namespace platform
} // namespace coconut

#endif // __APPLE__

#endif // COCONUT_PLATFORM_DARWIN_CREATE_WINDOW_H