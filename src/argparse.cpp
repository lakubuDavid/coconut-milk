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

  int i = 1;

  // Check for subcommand first (non-flag, non-option argument)
  if (i < argc && argv[i][0] != '-') {
    std::string_view sub = argv[i];
    if (sub == "generate") {
      args.generate = true;
      ++i;
    } else {
      std::println(stderr, "error: unknown subcommand '{}'", sub);
      printHelp(progname(argv[0]));
      std::exit(1);
    }
  }

  for (; i < argc; ++i) {
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

    if (a == "-o" || a == "--out-dir") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --out-dir requires a path argument");
        printGenerateHelp(progname(argv[0]));
        std::exit(1);
      }
      args.out_dir = argv[++i];
      continue;
    }

    // Unknown flag
    std::println(stderr, "error: unknown option '{}'", a);
    if (args.generate)
      printGenerateHelp(progname(argv[0]));
    else
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
  std::println("       {} generate [options]", progname(prog));
  std::println("");
  std::println("Run a Coconut Milk application.");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help       Show this help and exit");
  std::println("  -v, --version    Show version and exit");
  std::println("  -d, --debug      Enable developer tools / debug mode");
  std::println("  -r, --root PATH  Set project root directory (default: .)");
  std::println("");
  std::println("Subcommands:");
  std::println("  generate         Generate command wrappers from @command annotations");
  std::println("");
  std::println("The project root is searched for coconut.config.lua /");
  std::println("coconut.config.json and is the base for coconut:// assets.");
}

void printGenerateHelp(const char* prog) {
  std::println("Usage: {} generate [options]", progname(prog));
  std::println("");
  std::println("Parse all commands/*.lua for @command annotations and generate");
  std::println("type-safe wrappers (.g.lua, .g.js, .d.ts) plus an aggregated");
  std::println("commands.d.ts with a union type of all command names.");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help       Show this help and exit");
  std::println("  -o, --out-dir    Output directory (default: generated/)");
  std::println("");
  std::println("Runs from the project root. Reads coconut.config.* for");
  std::println("command_root and output_dir settings.");
}

void printVersion(const char* prog) {
  std::println("{} {}", progname(prog), VERSION);
}

} // namespace coconut::argparse
