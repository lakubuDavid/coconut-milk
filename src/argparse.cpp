#include "argparse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "print.h"
#include <string>
#include <string_view>
#include <vector>

namespace coconut::argparse {

// ---------------------------------------------------------------------------
// Table-driven option definitions
// ---------------------------------------------------------------------------

/// Describes a single command-line option (flag).
struct Option {
  std::string_view name;         ///< "--long-name" or "-s"
  bool             takes_value;  ///< whether this flag consumes the next arg
  const char*      help;         ///< help text line
  void (*apply)(Args&, const std::string& value);  ///< setter
};

/// Describes a subcommand.
struct Subcommand {
  std::string_view name;
  const char*      help;
  void (*apply)(Args&);          ///< sets the subcommand flag
  void (*printHelp)(const char* prog);
};

// ── Setter implementations ────────────────────────────────────────────────

static void setHelp(Args& a, const std::string&) { a.help = true; }
static void setVersion(Args& a, const std::string&) { a.version = true; }
static void setDebug(Args& a, const std::string&) { a.debug = true; }
static void setBytecode(Args& a, const std::string&) { a.bytecode_config = true; }
static void setWatch(Args& a, const std::string&) { a.watch = true; }
static void setFrameless(Args& a, const std::string&) { a.override_frameless = true; }
static void setTransparent(Args& a, const std::string&) { a.override_transparent = true; }
static void setYes(Args& a, const std::string&) { a.yes = true; }

static void setRoot(Args& a, const std::string& v) { a.root = v; }
static void setOutDir(Args& a, const std::string& v) { a.out_dir = v; }
static void setTemplate(Args& a, const std::string& v) { a.template_name = v; }
static void setTitle(Args& a, const std::string& v) { a.override_title_given = true; a.override_title = v; }
static void setWinWidth(Args& a, const std::string& v) { a.override_window_width = std::atoi(v.c_str()); }
static void setWinHeight(Args& a, const std::string& v) { a.override_window_height = std::atoi(v.c_str()); }

// Subcommand setters
static void setGenerate(Args& a) { a.generate = true; }
static void setBundle(Args& a) { a.bundle = true; }
static void setNew(Args& a) { a.new_cmd = true; }
static void setRun(Args& a) { a.run_cmd = true; }

// ── Option table ──────────────────────────────────────────────────────────

/// All flags, in display order (for --help).
/// Names starting with "--" are long options; "-x" are short options.
static const Option OPTIONS[] = {
  {"-h",        false, "Show this help and exit",                          setHelp},
  {"--help",    false, "Show this help and exit",                          setHelp},
  {"-v",        false, "Show version and exit",                            setVersion},
  {"--version", false, "Show version and exit",                            setVersion},
  {"-d",        false, "Enable developer tools / debug mode",              setDebug},
  {"--debug",   false, "Enable developer tools / debug mode",              setDebug},
  {"-r",        true,  "Set project root directory",                       setRoot},
  {"--root",    true,  "Set project root directory",                       setRoot},
  {"-o",        true,  "Output directory for subcommands",                 setOutDir},
  {"--out-dir", true,  "Output directory for subcommands",                 setOutDir},
  {"-t",        true,  "Template for 'new' subcommand",                    setTemplate},
  {"--template",true,  "Template for 'new' subcommand",                    setTemplate},
  {"--title",   true,  "Override window title for 'run'",                  setTitle},
  {"--window-width",  true,  "Override window width",                      setWinWidth},
  {"--window-height", true,  "Override window height",                     setWinHeight},
  {"--frameless",     false, "Enable frameless window",                    setFrameless},
  {"--transparent",   false, "Enable transparent window",                  setTransparent},
  {"--bytecode",      false, "Compile config to Lua bytecode (B2 opt-in)", setBytecode},
  {"-y",        false, "Skip prompts (for 'new' subcommand)",              setYes},
  {"--yes",     false, "Skip prompts (for 'new' subcommand)",              setYes},
  {"--watch",         false, "Watch for file changes and auto-regenerate", setWatch},
};

/// Subcommands, in display order.
static const Subcommand SUBCOMMANDS[] = {
  {"new",      "Scaffold a new Coconut Milk project",          setNew,      printNewHelp},
  {"run",      "Run a Coconut Milk application",               setRun,      printRunHelp},
  {"generate", "Generate command wrappers from @command annotations", setGenerate, printGenerateHelp},
  {"bundle",   "Package app into a standalone distributable bundle",    setBundle,    printBundleHelp},
};

// ── Lookup helpers ────────────────────────────────────────────────────────

/// Find an option by name. Returns nullptr if not found.
static const Option* findOption(std::string_view name) {
  for (const auto& opt : OPTIONS) {
    if (opt.name == name) return &opt;
  }
  return nullptr;
}

/// Find a subcommand by name. Returns nullptr if not found.
static const Subcommand* findSubcommand(std::string_view name) {
  for (const auto& sc : SUBCOMMANDS) {
    if (sc.name == name) return &sc;
  }
  return nullptr;
}

// ── Helpers ───────────────────────────────────────────────────────────────

static const char* progname(const char* argv0) {
  const char* slash = std::strrchr(argv0, '/');
  return slash ? slash + 1 : argv0;
}

// ── Parse ─────────────────────────────────────────────────────────────────

Args parse(int argc, char* argv[]) {
  Args args;

  // We process arguments in a single pass.
  // Positional args that aren't subcommands are collected as the project root.
  // Subcommands are detected and their flag is set.
  // Flags (starting with '-') are looked up in the option table.

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];

    // -- separator: stop flag parsing, rest are positional
    if (a == "--") {
      // Collect remaining as positional (currently unused, just stop)
      break;
    }

    // Subcommand detection (non-flag arg that matches a subcommand name)
    if (a[0] != '-') {
      const Subcommand* sc = findSubcommand(a);
      if (sc) {
        sc->apply(args);
        continue;
      }
      // Not a subcommand → positional.
      // First positional is the root directory; additional are app args.
      if (args.new_cmd) {
        args.new_name = a;
        args.positional_args.push_back(std::string(a));
      } else if (args.root == ".") {
        // First positional → set as root, not an app arg yet
        args.root = a;
      } else {
        // Additional positionals → app-level args
        args.positional_args.push_back(std::string(a));
      }
      continue;
    }

    // Flag parsing
    const Option* opt = findOption(a);
    if (!opt) {
      coconut::println(stderr, "error: unknown option '{}'", a);
      // Print relevant help
      for (const auto& sc : SUBCOMMANDS) {
        if (sc.apply == setGenerate && args.generate) { sc.printHelp(argv[0]); break; }
        if (sc.apply == setBundle   && args.bundle)   { sc.printHelp(argv[0]); break; }
        if (sc.apply == setNew      && args.new_cmd)  { sc.printHelp(argv[0]); break; }
        if (sc.apply == setRun      && args.run_cmd)  { sc.printHelp(argv[0]); break; }
      }
      printHelp(argv[0]);
      std::exit(1);
    }

    if (opt->takes_value) {
      if (i + 1 >= argc) {
        coconut::println(stderr, "error: '{}' requires a value", a);
        std::exit(1);
      }
      opt->apply(args, argv[++i]);
    } else {
      opt->apply(args, std::string{});
    }
  }

  return args;
}

// ---------------------------------------------------------------------------
// Help / Version
// ---------------------------------------------------------------------------

static void printCommonOptions() {
  coconut::println("Options:");
  for (const auto& opt : OPTIONS) {
    // Only show each option once (prefer long names, skip short duplicates)
    if (opt.name[0] == '-' && opt.name[1] == '-') {
      if (opt.takes_value)
        coconut::println("  {} <value>    {}", opt.name, opt.help);
      else
        coconut::println("  {}             {}", opt.name, opt.help);
    }
  }
}

void printHelp(const char* prog) {
  coconut::println("Usage: {} [options] [ROOT]", progname(prog));
  coconut::println("       {} <subcommand> [options]", progname(prog));
  coconut::println("");
  coconut::println("Run a Coconut Milk application or invoke a subcommand.");
  coconut::println("");
  coconut::println("Arguments:");
  coconut::println("  ROOT   Project root directory (default: .)");
  coconut::println("");
  printCommonOptions();
  coconut::println("");
  coconut::println("Subcommands:");
  for (const auto& sc : SUBCOMMANDS) {
    coconut::println("  {:<12} {}", sc.name, sc.help);
  }
  coconut::println("");
  coconut::println("The project root is searched for coconut.config.lua /");
  coconut::println("coconut.config.json and is the base for coconut:// assets.");
}

void printGenerateHelp(const char* prog) {
  coconut::println("Usage: {} generate [options]", progname(prog));
  coconut::println("");
  coconut::println("Parse all commands/*.lua for @command annotations and generate");
  coconut::println("type-safe wrappers (.g.lua, .g.js, .d.ts) plus an aggregated");
  coconut::println("commands.d.ts with a union type of all command names.");
  coconut::println("");
  coconut::println("Options:");
  coconut::println("  -h, --help       Show this help and exit");
  coconut::println("  -o, --out-dir    Output directory (default: generated/)");
  coconut::println("  --watch          Watch for file changes and auto-regenerate");
  coconut::println("");
  coconut::println("Runs from the project root. Reads coconut.config.* for");
  coconut::println("command_root and output_dir settings.");
}

void printBundleHelp(const char* prog) {
  coconut::println("Usage: {} bundle [options] [ROOT]", progname(prog));
  coconut::println("");
  coconut::println("Package the application into a standalone distributable bundle.");
  coconut::println("");
  coconut::println("The bundle command:");
  coconut::println("  1. Strips dev-only fields from coconut.config.* (debug, manifests)");
  coconut::println("  2. Writes the shippable config to the output directory");
  coconut::println("  3. Generates platform manifests (Info.plist, app.manifest, .desktop, metainfo.xml)");
  coconut::println("  4. Assembles .app directory structure, copies binary + assets");
  coconut::println("");
  coconut::println("Options:");
  coconut::println("  -h, --help       Show this help and exit");
  coconut::println("  -o, --out-dir    Output directory (default: bundle/)");
  coconut::println("  --bytecode       Compile stripped config to Lua bytecode (B2 opt-in)");
}

void printNewHelp(const char* prog) {
  coconut::println("Usage: {} new [name] [options]", progname(prog));
  coconut::println("");
  coconut::println("Scaffold a new Coconut Milk project.");
  coconut::println("");
  coconut::println("Creates the project directory with:");
  coconut::println("  coconut.config.lua   App configuration");
  coconut::println("  main.lua             Entry point script");
  coconut::println("  views/home.html      Default home view");
  coconut::println("  assets/style.css     Stylesheet");
  coconut::println("  assets/app.js        Frontend script");
  coconut::println("  commands/            Command folder");
  coconut::println("  generated/           Generated files");
  coconut::println("");
  coconut::println("Options:");
  coconut::println("  -h, --help           Show this help and exit");
  coconut::println("  -y, --yes             Skip prompts, use defaults");
  coconut::println("  -t, --template NAME   Template: bare (default), bare-ts, vite");
}

void printRunHelp(const char* prog) {
  coconut::println("Usage: {} run [options] [ROOT]", progname(prog));
  coconut::println("");
  coconut::println("Run a Coconut Milk application.");
  coconut::println("");
  coconut::println("Auto-detects the project by looking for coconut.config.lua");
  coconut::println("in the current directory or ROOT.");
  coconut::println("");
  printCommonOptions();
}

void printVersion(const char* prog) {
  coconut::println("{} {}", progname(prog), VERSION);
}

} // namespace coconut::argparse
