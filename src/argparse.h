#pragma once

#include <string>
#include <string_view>

namespace coconut::argparse {

/// Coconut Milk version string.
inline constexpr std::string_view VERSION = "0.1.0";

/// Parsed command-line arguments.
struct Args {
  bool help      = false;
  bool version   = false;
  bool debug     = false;
  bool generate  = false;  ///< "generate" subcommand
  std::string    root = ".";  ///< project root directory (default: CWD)
  std::string    out_dir = "generated";  ///< output dir for generate subcommand
};

/// Parse command-line arguments.
/// Exits on unknown flags (prints error + help to stderr).
Args parse(int argc, char* argv[]);

/// Print runtime usage to stdout.
void printHelp(const char* prog);

/// Print generate subcommand usage to stdout.
void printGenerateHelp(const char* prog);

/// Print version to stdout.
void printVersion(const char* prog);

} // namespace coconut::argparse
