#pragma once

#include <string>
#include <vector>

namespace coconut {

#ifndef COCONUT_VERSION
#define COCONUT_VERSION "0.1.1"
#endif

  /// Coconut Milk version string.
  inline constexpr std::string_view VERSION = COCONUT_VERSION;

  /// Parsed command-line arguments.
  struct Args {
    bool        help            = false;
    bool        version         = false;
    bool        debug           = false;
    bool        generate        = false;      ///< "generate" subcommand
    bool        bundle          = false;      ///< "bundle" subcommand
    bool        new_cmd         = false;      ///< "new" subcommand
    bool        run_cmd         = false;      ///< "run" subcommand
    bool        watch           = false;      ///< watch mode (generate --watch)
    bool        bytecode_config = false;      ///< compile config to .luac (B2 opt-in)
    std::string root            = ".";        ///< project root directory (default: CWD)
    std::string new_name;                     ///< project name for "new" subcommand
    std::string template_name = "default";    ///< template for "new" subcommand
    bool        yes           = false;        ///< --yes flag (skip prompts)
    std::string out_dir       = "generated";  ///< output dir for subcommands

    // Config override flags (for "run" and default mode)
    int         override_window_width  = 0;      ///< 0 = use config value
    int         override_window_height = 0;      ///< 0 = use config value
    bool        override_frameless     = false;  ///< --frameless flag
    bool        override_transparent   = false;  ///< --transparent flag
    bool        override_title_given   = false;  ///< true when --title is supplied
    std::string override_title;                  ///< --title value

    // Positional arguments (app-level, not framework flags)
    std::vector<std::string> positional_args;
  };

  /// Parse command-line arguments.
  /// Exits (via p-ranav) on `--help`; propagates exceptions as a clean error +
  /// usage and std::exit(1) on unknown options.
  Args parseArguments(int argc, char* argv[]);

  /// Print version to stdout.
  void printVersion(const char* prog);

}  // namespace coconut
