#pragma once

#include <string>
#include <string_view>

namespace coconut::argparse {

/// Coconut Milk version string.
inline constexpr std::string_view VERSION = "0.1.0";

/// Parsed command-line arguments.
struct Args {
  bool help    = false;
  bool version = false;
  bool debug   = false;
  std::string  root = ".";  ///< project root directory (default: CWD)
};

/// Parse command-line arguments.
/// Exits on unknown flags (prints error + help to stderr).
Args parse(int argc, char* argv[]);

/// Print usage to stdout.
void printHelp(const char* prog);

/// Print version to stdout.
void printVersion(const char* prog);

} // namespace coconut::argparse
