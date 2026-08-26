#include "core/env.h"
#include "test.h"

#include <string>

// ── Cross-platform env API tests ──────────────────────────────────────
// Environment variables and system paths should work on all platforms.

COCONUT_TEST(unit, env_homedir_returns_nonempty) {
  // Homedir is always set on every OS
  auto dir = coconut::core::env::homedir();
  COCONUT_REQUIRE(!dir.empty());
}

COCONUT_TEST(unit, env_homedir_is_absolute) {
  // Homedir is always an absolute path
  auto dir = coconut::core::env::homedir();
  COCONUT_REQUIRE_EQ(dir[0], '/');
}

COCONUT_TEST(unit, env_cwd_returns_nonempty) {
  auto cwd = coconut::core::env::cwd();
  COCONUT_REQUIRE(!cwd.empty());
}

COCONUT_TEST(unit, env_cwd_is_absolute) {
  auto cwd = coconut::core::env::cwd();
  COCONUT_REQUIRE_EQ(cwd[0], '/');
}

COCONUT_TEST(unit, env_get_home) {
  auto home = coconut::core::env::get("HOME");
  COCONUT_REQUIRE(!home.empty());
}

COCONUT_TEST(unit, env_get_nonexistent_returns_empty) {
  auto result = coconut::core::env::get("__COCONUT_NONEXISTENT_VAR__");
  COCONUT_REQUIRE(result.empty());
}

COCONUT_TEST(unit, env_get_empty_name_returns_empty) {
  auto result = coconut::core::env::get("");
  COCONUT_REQUIRE(result.empty());
}

COCONUT_TEST(unit, env_path_separator) {
  // ':' on macOS/Linux, ';' on Windows
  char sep = coconut::core::env::pathSeparator();
#if defined(_WIN32)
  COCONUT_REQUIRE_EQ(sep, ';');
#else
  COCONUT_REQUIRE_EQ(sep, ':');
#endif
}

COCONUT_TEST(unit, env_home_and_homedir_match) {
  auto envHome = coconut::core::env::get("HOME");
  auto libHome = coconut::core::env::homedir();
  COCONUT_REQUIRE_EQ(envHome, libHome);
}

COCONUT_TEST(unit, env_get_path) {
  auto path = coconut::core::env::get("PATH");
  COCONUT_REQUIRE(!path.empty());
  COCONUT_REQUIRE(path.length() > 5);  // PATH is never just empty
}
