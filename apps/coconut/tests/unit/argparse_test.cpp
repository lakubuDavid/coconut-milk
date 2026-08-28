#include "argparse.h"
#include "test.h"

#include <string>
#include <vector>

// ── Help / version flags ──────────────────────────────────────────────

COCONUT_TEST(unit, argparse_help_flag) {
  const char* argv[] = {"coconut", "--help", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.help);
}

COCONUT_TEST(unit, argparse_version_flag) {
  const char* argv[] = {"coconut", "--version", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.version);
}

// ── Debug mode ────────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_debug_flag) {
  const char* argv[] = {"coconut", "--debug", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.debug);
}

// ── Generate subcommand ───────────────────────────────────────────────

COCONUT_TEST(unit, argparse_generate_subcommand) {
  const char* argv[] = {"coconut", "generate", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.generate);
}

COCONUT_TEST(unit, argparse_generate_with_watch) {
  const char* argv[] = {"coconut", "generate", "--watch", nullptr};
  auto        args   = coconut::parseArguments(3, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.generate);
  COCONUT_REQUIRE(args.watch);
}

COCONUT_TEST(unit, argparse_generate_with_outdir) {
  const char* argv[] = {"coconut", "generate", "--out-dir", "build/gen", nullptr};
  auto        args   = coconut::parseArguments(4, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.generate);
  COCONUT_REQUIRE_EQ(args.out_dir, std::string("build/gen"));
}

// ── Bundle subcommand ─────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_bundle_subcommand) {
  const char* argv[] = {"coconut", "bundle", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.bundle);
}

// ── New subcommand ────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_new_subcommand) {
  const char* argv[] = {"coconut", "new", "myapp", nullptr};
  auto        args   = coconut::parseArguments(3, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.new_cmd);
  COCONUT_REQUIRE_EQ(args.new_name, std::string("myapp"));
}

COCONUT_TEST(unit, argparse_new_with_template) {
  const char* argv[] = {"coconut", "new", "myapp", "--template", "minimal", nullptr};
  auto        args   = coconut::parseArguments(5, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.new_cmd);
  COCONUT_REQUIRE_EQ(args.new_name, std::string("myapp"));
  COCONUT_REQUIRE_EQ(args.template_name, std::string("minimal"));
}

// ── Run subcommand ────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_run_subcommand) {
  const char* argv[] = {"coconut", "run", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.run_cmd);
}

COCONUT_TEST(unit, argparse_run_with_override_flags) {
  const char* argv[] = {
      "coconut",
      "run",
      "--window-width",
      "1024",
      "--window-height",
      "768",
      "--title",
      "My App",
      "--frameless",
      "--debug",
      nullptr
  };
  auto args = coconut::parseArguments(10, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.run_cmd);
  COCONUT_REQUIRE_EQ(args.override_window_width, 1024);
  COCONUT_REQUIRE_EQ(args.override_window_height, 768);
  COCONUT_REQUIRE(args.override_title_given);
  COCONUT_REQUIRE_EQ(args.override_title, std::string("My App"));
  COCONUT_REQUIRE(args.override_frameless);
  COCONUT_REQUIRE(args.debug);
}

// ── Root directory ────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_root_flag) {
  const char* argv[] = {"coconut", "--root", "/some/project", nullptr};
  auto        args   = coconut::parseArguments(3, const_cast<char**>(argv));
  COCONUT_REQUIRE_EQ(args.root, std::string("/some/project"));
}

// ── Defaults ──────────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_defaults) {
  const char* argv[] = {"coconut", nullptr};
  auto        args   = coconut::parseArguments(1, const_cast<char**>(argv));
  COCONUT_REQUIRE(!args.help);
  COCONUT_REQUIRE(!args.version);
  COCONUT_REQUIRE(!args.debug);
  COCONUT_REQUIRE(!args.generate);
  COCONUT_REQUIRE(!args.bundle);
  COCONUT_REQUIRE(!args.new_cmd);
  COCONUT_REQUIRE(!args.run_cmd);
  COCONUT_REQUIRE(!args.watch);
  COCONUT_REQUIRE(!args.bytecode_config);
  COCONUT_REQUIRE(!args.yes);
  COCONUT_REQUIRE_EQ(args.root, std::string("."));
  COCONUT_REQUIRE_EQ(args.out_dir, std::string("generated"));
  COCONUT_REQUIRE_EQ(args.template_name, std::string("default"));
  COCONUT_REQUIRE_EQ(args.override_window_width, 0);
  COCONUT_REQUIRE_EQ(args.override_window_height, 0);
  COCONUT_REQUIRE(!args.override_frameless);
  COCONUT_REQUIRE(!args.override_transparent);
  COCONUT_REQUIRE(!args.override_title_given);
  COCONUT_REQUIRE(args.override_title.empty());
  COCONUT_REQUIRE(args.positional_args.empty());
}

// ── Positional arguments ──────────────────────────────────────────────

COCONUT_TEST(unit, argparse_positional_args) {
  // First positional arg becomes the root, subsequent ones are app args.
  const char* argv[] = {"coconut", "dir1", "file1.lua", nullptr};
  auto        args   = coconut::parseArguments(3, const_cast<char**>(argv));
  COCONUT_REQUIRE_EQ(args.root, std::string("dir1"));
  COCONUT_REQUIRE_EQ(args.positional_args.size(), size_t(1));
  COCONUT_REQUIRE_EQ(args.positional_args[0], std::string("file1.lua"));
}

// ── Bytecode flag ─────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_bytecode_flag) {
  const char* argv[] = {"coconut", "--bytecode", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.bytecode_config);
}

// ── Yes flag ──────────────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_yes_flag) {
  const char* argv[] = {"coconut", "--yes", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.yes);
}

// ── Transparent flag ──────────────────────────────────────────────────

COCONUT_TEST(unit, argparse_transparent_flag) {
  const char* argv[] = {"coconut", "--transparent", nullptr};
  auto        args   = coconut::parseArguments(2, const_cast<char**>(argv));
  COCONUT_REQUIRE(args.override_transparent);
}
