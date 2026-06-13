#include "config.h"
#include "fs.h"
#include "test.h"

#include <cstdio>
#include <string>
#include <vector>

// ── Tools ─────────────────────────────────────────────────────────────

// Helper: write a temp file path in system tmp
static std::string tempPath(const char* name) {
  return std::string("/tmp/coconut-test-") + name;
}

static void cleanup(const std::string& path) {
  std::remove(path.c_str());
}

// ── Roots creation ────────────────────────────────────────────────────

COCONUT_TEST(unit, fs_roots_create_and_destroy) {
  coconut::Config config{};

  auto result = coconut::fs::create(&config);
  COCONUT_REQUIRE(result);
  coconut::fs::Roots* roots = result.value();

  COCONUT_REQUIRE(roots != nullptr);
  COCONUT_REQUIRE(roots->configs == &config);
  COCONUT_REQUIRE_EQ(roots->view_root, std::string("views"));
  COCONUT_REQUIRE_EQ(roots->asset_root, std::string("assets"));
  COCONUT_REQUIRE_EQ(roots->command_root, std::string("commands"));

  coconut::fs::destroy(roots);
}

COCONUT_TEST(unit, fs_roots_destroy_null) {
  // Must not crash
  coconut::fs::destroy(nullptr);
}

COCONUT_TEST(unit, fs_create_null_config) {
  // Must return error for null config
  auto result = coconut::fs::create(nullptr);
  COCONUT_REQUIRE(!result.has_value());
}

// ── File existence ────────────────────────────────────────────────────

COCONUT_TEST(unit, fs_exists_nonexistent) {
  COCONUT_REQUIRE(!coconut::fs::exists("/tmp/coconut-nonexistent-file"));
}

COCONUT_TEST(unit, fs_exists_empty_path) {
  COCONUT_REQUIRE(!coconut::fs::exists(""));
}

COCONUT_TEST(unit, fs_exists_existing_path) {
  // /tmp always exists on Unix
  COCONUT_REQUIRE(coconut::fs::exists("/tmp"));
}

// ── Read/Write text ───────────────────────────────────────────────────

COCONUT_TEST(unit, fs_write_text) {
  auto path = tempPath("write-text");
  cleanup(path);  // ensure clean slate

  auto result = coconut::fs::writeText(path, "hello coconut");
  COCONUT_REQUIRE(result.has_value());
  cleanup(path);
}

COCONUT_TEST(unit, fs_read_text_empty_file) {
  auto path = tempPath("read-empty");
  cleanup(path);
  coconut::fs::writeText(path, "");
  auto result = coconut::fs::readText(path);
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result.value().empty());
  cleanup(path);
}

COCONUT_TEST(unit, fs_write_then_read) {
  auto path = tempPath("roundtrip");
  cleanup(path);

  coconut::fs::writeText(path, "line1\nline2\nline3");
  auto result = coconut::fs::readText(path);
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE_EQ(result.value(), std::string("line1\nline2\nline3"));

  cleanup(path);
}

COCONUT_TEST(unit, fs_read_nonexistent) {
  auto result = coconut::fs::readText("/tmp/coconut-nonexistent");
  COCONUT_REQUIRE(!result.has_value());
}

// ── Read/Write bytes ──────────────────────────────────────────────────

COCONUT_TEST(unit, fs_write_bytes) {
  auto path = tempPath("write-bytes");
  cleanup(path);

  std::vector<uint8_t> data = {0x00, 0xFF, 0xAB, 0xCD};
  auto result = coconut::fs::writeBytes(path, data);
  COCONUT_REQUIRE(result.has_value());

  cleanup(path);
}

COCONUT_TEST(unit, fs_read_bytes) {
  auto path = tempPath("read-bytes");
  cleanup(path);

  std::vector<uint8_t> expected = {0x01, 0x02, 0x03};
  coconut::fs::writeBytes(path, expected);
  auto result = coconut::fs::readBytes(path);
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE_EQ(result.value().size(), expected.size());
  COCONUT_REQUIRE_EQ(result.value()[0], uint8_t(0x01));

  cleanup(path);
}

// ── Path resolution ───────────────────────────────────────────────────

COCONUT_TEST(unit, fs_resolve_relative) {
  auto resolved = coconut::fs::resolve("/root", "sub/file.txt");
  COCONUT_REQUIRE_EQ(resolved, std::string("/root/sub/file.txt"));
}

COCONUT_TEST(unit, fs_resolve_absolute_stays_absolute) {
  auto resolved = coconut::fs::resolve("/root", "/abs/path.txt");
  COCONUT_REQUIRE_EQ(resolved, std::string("/abs/path.txt"));
}

COCONUT_TEST(unit, fs_resolve_empty_relpath) {
  auto resolved = coconut::fs::resolve("/root", "");
  COCONUT_REQUIRE_EQ(resolved, std::string("/root/"));
}

COCONUT_TEST(unit, fs_resolve_dot_as_absolute) {
  // "." is not absolute, should resolve against root
  auto resolved = coconut::fs::resolve("/base", ".");
  COCONUT_REQUIRE_EQ(resolved, std::string("/base/."));
}

// ── List directory ────────────────────────────────────────────────────

COCONUT_TEST(unit, fs_list_dir_root) {
  auto result = coconut::fs::listDir("/");
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(!result->empty());  // root always has entries
}

COCONUT_TEST(unit, fs_list_dir_contains_dotfiles) {
  // Common entries that should exist
  auto result = coconut::fs::listDir("/tmp");
  COCONUT_REQUIRE(result.has_value());
  bool found = false;
  for (const auto& entry : result.value()) {
    if (entry.name == "." || entry.name == "..") found = true;
  }
  COCONUT_REQUIRE(found);
}

COCONUT_TEST(unit, fs_list_dir_types) {
  auto result = coconut::fs::listDir("/");
  COCONUT_REQUIRE(result.has_value());
  for (const auto& entry : result.value()) {
    COCONUT_REQUIRE(!entry.name.empty());
    COCONUT_REQUIRE(!entry.path.empty());
    // is_dir must be either true or false
    COCONUT_REQUIRE(entry.is_dir == true || entry.is_dir == false);
  }
}

COCONUT_TEST(unit, fs_list_dir_nonexistent) {
  auto result = coconut::fs::listDir("/tmp/coconut-nonexistent-dir");
  COCONUT_REQUIRE(!result.has_value());
}
