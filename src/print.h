#ifndef COCONUT_PRINT_H
#define COCONUT_PRINT_H

/// Portable print / format wrappers.
///
/// Selects the best available implementation based on what the
/// compiler / standard library supports:
///
///   | Feature          | macOS (clang++ + libc++) | Linux (g++ + libstdc++) | Windows (MSVC STL) |
///   |------------------|--------------------------|-------------------------|---------------------|
///   | std::format      | ✅ C++20                 | ✅ GCC 13+ (C++23)      | ✅ MSVC 2022 17.0+  |
///   | std::println     | ✅ C++23                 | ❌ GCC 13 (needs 14+)   | ✅ MSVC 2022 17.5+  |
///
/// Usage:
///   coconut::println("Hello, {}!", name);
///   coconut::println(std::cerr, "error: {}", msg);
///   auto s = coconut::format("{} + {} = {}", a, b, a + b);

#include <format>
#include <iostream>
#include <string>

// ── Feature detection ─────────────────────────────────────────────────
// std::println / std::print are in <print> (C++23, P2093).
// libc++ 17+ and libstdc++ 14+ provide it.
#if __has_include(<print>)
#  include <print>
#  define COCONUT_HAS_PRINTLN 1
#endif

namespace coconut {

// ── println ───────────────────────────────────────────────────────────

/// Print a formatted line to stdout.
template<typename... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
#if COCONUT_HAS_PRINTLN
  std::println(fmt, std::forward<Args>(args)...);
#else
  std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
#endif
}

/// Print a formatted line to an arbitrary output stream.
template<typename... Args>
void println(std::ostream& os, std::format_string<Args...> fmt, Args&&... args) {
#if COCONUT_HAS_PRINTLN
  std::println(os, fmt, std::forward<Args>(args)...);
#else
  os << std::format(fmt, std::forward<Args>(args)...) << std::endl;
#endif
}

// ── format ────────────────────────────────────────────────────────────
// std::format is available on all our target platforms (C++23 mode).
// We just re-export it for convenience / single-include.
using std::format;

} // namespace coconut

#endif // COCONUT_PRINT_H
