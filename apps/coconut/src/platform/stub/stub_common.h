/// Stub platform implementations for NO_PLATFORM builds.
///
/// These provide linker-satisfying definitions for every platform function
/// without pulling in Cocoa, WebKit, GTK, or any OS-specific frameworks.
/// Module test binaries define NO_PLATFORM and link these stubs instead
/// of the real platform .cpp files.
///
/// No dispatcher file is modified — the real headers (e.g. platform/darwin/window.h)
/// still supply the declarations at compile time.  The stubs supply the definitions
/// at link time.

#ifndef COCONUT_PLATFORM_STUB_COMMON_H
#define COCONUT_PLATFORM_STUB_COMMON_H

#ifdef NO_PLATFORM

// Suppress "unused parameter" warnings in stub functions.
#define STUB_UNUSED(x) (void)(x)

#else
#error "stub_common.h should only be included in NO_PLATFORM builds"
#endif

#endif // COCONUT_PLATFORM_STUB_COMMON_H
