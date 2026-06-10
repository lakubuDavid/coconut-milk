#include "argparse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <string_view>

namespace coconut::argparse {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* progname(const char* argv0) {
  const char* slash = std::strrchr(argv0, '/');
  return slash ? slash + 1 : argv0;
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

Args parse(int argc, char* argv[]) {
  Args args;

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];

    if (a == "-h" || a == "--help") {
      args.help = true;
      return args;  // help requested, stop parsing
    }

    if (a == "-v" || a == "--version") {
      args.version = true;
      return args;  // version requested, stop parsing
    }

    if (a == "-d" || a == "--debug") {
      args.debug = true;
      continue;
    }

    if (a == "-r" || a == "--root") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --root requires a path argument");
        printHelp(progname(argv[0]));
        std::exit(1);
      }
      args.root = argv[++i];
      continue;
    }

    // Unknown flag
    std::println(stderr, "error: unknown option '{}'", a);
    printHelp(progname(argv[0]));
    std::exit(1);
  }

  return args;
}

// ---------------------------------------------------------------------------
// Help / Version
// ---------------------------------------------------------------------------

void printHelp(const char* prog) {
  std::println("Usage: {} [options]", progname(prog));
  std::println("");
  std::println("Options:");
  std::println("  -h, --help       Show this help and exit");
  std::println("  -v, --version    Show version and exit");
  std::println("  -d, --debug      Enable developer tools / debug mode");
  std::println("  -r, --root PATH  Set project root directory (default: .)");
  std::println("");
  std::println("The project root is searched for coconut.config.lua /");
  std::println("coconut.config.json and is the base for coconut:// assets.");
}

void printVersion(const char* prog) {
  std::println("{} {}", progname(prog), VERSION);
}

} // namespace coconut::argparse
