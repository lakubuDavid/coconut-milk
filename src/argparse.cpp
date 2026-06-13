#include "argparse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <string_view>
#include <vector>

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

  // Collect all positional (non-flag, non-option) arguments.
  // The first one that isn't a known subcommand name is the project root.
  // e.g. `coconut /path/to/project`  or  `coconut generate /path/to/other`
  std::vector<std::string> positional;
  for (int j = 1; j < argc; ++j) {
    std::string_view a = argv[j];
    // Skip flags AND their values
    if (a == "-r" || a == "--root" || a == "-o" || a == "--out-dir" ||
        a == "--window-width" || a == "--window-height" || a == "--title" ||
        a == "-t" || a == "--template") {
      ++j;  // skip the value arg too
      continue;
    }
    if (a == "-h" || a == "--help" || a == "-v" || a == "--version" ||
        a == "-d" || a == "--debug" || a == "--bytecode" ||
        a == "--frameless" || a == "--transparent" || a == "--watch") {
      continue;  // flag, no value
    }
    if (a == "generate") {
      args.generate = true;
      continue;
    }
    if (a == "bundle") {
      args.bundle = true;
      continue;
    }
    if (a == "new") {
      args.new_cmd = true;
      continue;
    }
    if (a == "run") {
      args.run_cmd = true;
      continue;
    }
    if (a[0] != '-') {
      positional.push_back(argv[j]);
    }
  }

  // First positional arg is either the project name (for "new") or
  // the project root (for all other modes).
  // Flag-based --root overrides the root.
  bool root_given_by_flag = false;
  for (int j = 1; j < argc; ++j) {
    if (std::string_view(argv[j]) == "-r" || std::string_view(argv[j]) == "--root") {
      root_given_by_flag = true;
      break;
    }
  }
  if (!positional.empty()) {
    if (args.new_cmd) {
      args.new_name = positional[0];
    } else if (!root_given_by_flag) {
      args.root = positional[0];
    }
  }

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];

    // Skip known non-flag positional args (root / generate)
    if (a == "generate") {
      args.generate = true;
      continue;
    }
    if (a == "bundle") {
      args.bundle = true;
      continue;
    }
    if (a == "new") {
      args.new_cmd = true;
      continue;
    }
    if (a == "run") {
      args.run_cmd = true;
      continue;
    }
    if (a[0] != '-') {
      continue;  // already handled as positional root
    }
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
        std::exit(1);
      }
      args.out_dir = argv[++i];
      continue;
    }

    if (a == "--bytecode") {
      args.bytecode_config = true;
      continue;
    }

    if (a == "--watch") {
      args.watch = true;
      continue;
    }

    if (a == "-t" || a == "--template") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --template requires a name");
        std::exit(1);
      }
      args.template_name = argv[++i];
      continue;
    }

    // Config override flags (for run / default mode)
    if (a == "--window-width") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --window-width requires a number");
        std::exit(1);
      }
      args.override_window_width = std::atoi(argv[++i]);
      continue;
    }
    if (a == "--window-height") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --window-height requires a number");
        std::exit(1);
      }
      args.override_window_height = std::atoi(argv[++i]);
      continue;
    }
    if (a == "--frameless") {
      args.override_frameless = true;
      continue;
    }
    if (a == "--transparent") {
      args.override_transparent = true;
      continue;
    }
    if (a == "--title") {
      if (i + 1 >= argc) {
        std::println(stderr, "error: --title requires a string");
        std::exit(1);
      }
      args.override_title_given = true;
      args.override_title = argv[++i];
      continue;
    }

    // Unknown flag
    std::println(stderr, "error: unknown option '{}'", a);
    if (args.generate)      printGenerateHelp(progname(argv[0]));
    else if (args.bundle)   printBundleHelp(progname(argv[0]));
    else if (args.new_cmd)  printNewHelp(progname(argv[0]));
    else if (args.run_cmd)  printRunHelp(progname(argv[0]));
    else                    printHelp(progname(argv[0]));
    std::exit(1);
  }

  return args;
}

// ---------------------------------------------------------------------------
// Help / Version
// ---------------------------------------------------------------------------

void printHelp(const char* prog) {
  std::println("Usage: {} [options] [ROOT]", progname(prog));
  std::println("       {} <subcommand> [options]", progname(prog));
  std::println("");
  std::println("Run a Coconut Milk application or invoke a subcommand.");
  std::println("");
  std::println("Arguments:");
  std::println("  ROOT   Project root directory (default: .)");
  std::println("");
  std::println("Run options:");
  std::println("  -h, --help           Show this help and exit");
  std::println("  -v, --version        Show version and exit");
  std::println("  -d, --debug          Enable developer tools / debug mode");
  std::println("  -r, --root PATH      Set project root directory");
  std::println("  --window-width N     Override window width");
  std::println("  --window-height N    Override window height");
  std::println("  --frameless          Enable frameless window");
  std::println("  --transparent        Enable transparent window");
  std::println("  --title STR          Override window title");
  std::println("");
  std::println("Subcommands:");
  std::println("  new <name>    Scaffold a new Coconut Milk project");
  std::println("  run [ROOT]    Run the app (default when no subcommand)");
  std::println("  generate      Generate command wrappers from @command annotations");
  std::println("  bundle        Package app into a standalone distributable bundle");
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
  std::println("  --watch          Watch for file changes and auto-regenerate");
  std::println("");
  std::println("Runs from the project root. Reads coconut.config.* for");
  std::println("command_root and output_dir settings.");
}

void printBundleHelp(const char* prog) {
  std::println("Usage: {} bundle [options] [ROOT]", progname(prog));
  std::println("");
  std::println("Package the application into a standalone distributable bundle.");
  std::println("");
  std::println("The bundle command:");
  std::println("  1. Strips dev-only fields from coconut.config.* (debug, manifests)");
  std::println("  2. Writes the shippable config to the output directory");
  std::println("  3. Generates platform manifests (Info.plist, app.manifest, .desktop, metainfo.xml)");
  std::println("  4. Assembles .app directory structure, copies binary + assets");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help       Show this help and exit");
  std::println("  -o, --out-dir    Output directory (default: bundle/)");
  std::println("  --bytecode       Compile stripped config to Lua bytecode (B2 opt-in)");
}

void printNewHelp(const char* prog) {
  std::println("Usage: {} new <name> [options]", progname(prog));
  std::println("");
  std::println("Scaffold a new Coconut Milk project.");
  std::println("");
  std::println("Creates the project directory with:");
  std::println("  coconut.config.lua   App configuration");
  std::println("  main.lua             Entry point script");
  std::println("  views/home.html      Default home view");
  std::println("  assets/style.css     Stylesheet");
  std::println("  assets/app.js        Frontend script");
  std::println("  commands/            Command folder");
  std::println("  generated/           Generated files");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help           Show this help and exit");
  std::println("  --template NAME      Template: default (default), minimal");
}

void printRunHelp(const char* prog) {
  std::println("Usage: {} run [options] [ROOT]", progname(prog));
  std::println("");
  std::println("Run a Coconut Milk application.");
  std::println("");
  std::println("Auto-detects the project by looking for coconut.config.lua");
  std::println("in the current directory or ROOT.");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help           Show this help and exit");
  std::println("  -d, --debug          Enable developer tools / debug mode");
  std::println("  -r, --root PATH      Set project root directory");
  std::println("  --window-width N     Override window width");
  std::println("  --window-height N    Override window height");
  std::println("  --frameless          Enable frameless window");
  std::println("  --transparent        Enable transparent window");
  std::println("  --title STR          Override window title");
}

void printVersion(const char* prog) {
  std::println("{} {}", progname(prog), VERSION);
}

} // namespace coconut::argparse
