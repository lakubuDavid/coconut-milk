#include "argparse.h"

#include <argparse/argparse.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "print.h"

// The p-ranav library lives in ::argparse; alias it so references inside
// namespace coconut::argparse resolve correctly.

namespace coconut {

  namespace {

    /// Add the shared option set (the global flags that apply in every mode) to a
    /// parser. Applied both to the root parser (run mode) and to each subparser so
    /// that `coconut <subcommand> --debug` etc. behaves like the old global parser.
    void addCommonOptions(argparse::ArgumentParser& p) {
      p.add_argument("-h", "--help").help("Show this help and exit").flag();
      p.add_argument("-v", "--version").help("Show version and exit").flag();
      p.add_argument("-d", "--debug").help("Enable developer tools / debug mode").flag();
      p.add_argument("-r", "--root")
          .help("Set project root directory")
          .default_value(std::string("."));
      p.add_argument("-o", "--out-dir")
          .help("Output directory for subcommands")
          .default_value(std::string("generated"));
      p.add_argument("-t", "--template")
          .help("Template for 'new' subcommand")
          .default_value(std::string("default"));
      p.add_argument("--title")
          .help("Override window title for 'run'")
          .default_value(std::string(""));
      p.add_argument("--window-width")
          .help("Override window width")
          .scan<'i', int>()
          .default_value(0);
      p.add_argument("--window-height")
          .help("Override window height")
          .scan<'i', int>()
          .default_value(0);
      p.add_argument("--frameless").help("Enable frameless window").flag();
      p.add_argument("--transparent").help("Enable transparent window").flag();
      p.add_argument("--bytecode").help("Compile config to Lua bytecode (B2 opt-in)").flag();
      p.add_argument("-y", "--yes").help("Skip prompts (for 'new' subcommand)").flag();
      p.add_argument("--watch").help("Watch for file changes and auto-regenerate").flag();
    }

  }  // namespace

  Args parseArguments(int argc, char* argv[]) {
    Args args;

    // Root parser (run / default mode).
    // default_arguments::none: p-ranav auto-adds -h/--help and -v/--version
    // (which std::exit(0)); we add them manually as flags in addCommonOptions so
    // --help/--version set Args fields instead of exiting (keeps in-process
    // tests + main flow intact).
    argparse::ArgumentParser program(
        "coconut", std::string{COCONUT_VERSION}, argparse::default_arguments::none, false
    );
    addCommonOptions(program);

    // Subparsers.
    argparse::ArgumentParser generate("generate", "", argparse::default_arguments::none, false);
    generate.add_description("Generate command wrappers from @command annotations");
    addCommonOptions(generate);

    argparse::ArgumentParser bundle("bundle", "", argparse::default_arguments::none, false);
    bundle.add_description("Package app into a standalone distributable bundle");
    addCommonOptions(bundle);

    argparse::ArgumentParser new_cmd("new", "", argparse::default_arguments::none, false);
    new_cmd.add_description("Scaffold a new Coconut Milk project");
    addCommonOptions(new_cmd);
    new_cmd.add_argument("name")
        .help("Project name to scaffold")
        .nargs(0, 1)
        .default_value(std::string(""));

    argparse::ArgumentParser run("run", "", argparse::default_arguments::none, false);
    run.add_description("Run a Coconut Milk application");
    addCommonOptions(run);

    program.add_subparser(generate);
    program.add_subparser(bundle);
    program.add_subparser(new_cmd);
    program.add_subparser(run);

    // parse_known_args returns the unrecognized leftovers (used for run-mode
    // root + app pass-through args). Unknown options still throw.
    std::vector<std::string> remaining;
    try {
      remaining = program.parse_known_args(argc, argv);
    } catch (const std::exception& err) {
      coconut::println(std::cerr, "{}", err.what());
      std::cerr << program;
      std::exit(1);
    }

    // Subcommand flags.
    args.generate = program.is_subcommand_used("generate");
    args.bundle   = program.is_subcommand_used("bundle");
    args.new_cmd  = program.is_subcommand_used("new");
    args.run_cmd  = program.is_subcommand_used("run");

    // The active parser is the subparser when a subcommand was used, else the
    // root parser. Shared options are registered on every parser, so read them
    // from whichever one actually consumed the command line.
    argparse::ArgumentParser* active = &program;
    if (args.generate)
      active = &generate;
    else if (args.bundle)
      active = &bundle;
    else if (args.new_cmd)
      active = &new_cmd;
    else if (args.run_cmd)
      active = &run;

    // Help (manual flag so we don't auto-exit — keeps in-process tests + main
    // flow intact). Print the relevant parser's help; main.cpp returns 0.
    args.help = active->get<bool>("--help");
    if (args.help) {
      if (args.generate)
        std::cout << generate << "\n";
      else if (args.bundle)
        std::cout << bundle << "\n";
      else if (args.new_cmd)
        std::cout << new_cmd << "\n";
      else if (args.run_cmd)
        std::cout << run << "\n";
      else
        std::cout << program << "\n";
    }

    // Version / debug.
    args.version = active->get<bool>("--version");
    args.debug   = active->get<bool>("--debug");

    // Shared value options (readable from the active parser even when supplied
    // under a subcommand).
    args.out_dir                = active->get<std::string>("--out-dir");
    args.template_name          = active->get<std::string>("--template");
    args.yes                    = active->get<bool>("--yes");
    args.watch                  = active->get<bool>("--watch");
    args.bytecode_config        = active->get<bool>("--bytecode");
    args.override_frameless     = active->get<bool>("--frameless");
    args.override_transparent   = active->get<bool>("--transparent");
    args.override_window_width  = active->get<int>("--window-width");
    args.override_window_height = active->get<int>("--window-height");

    const std::string title = active->get<std::string>("--title");
    if (!title.empty()) {
      args.override_title_given = true;
      args.override_title       = title;
    }

    const std::string opt_root = active->get<std::string>("--root");

    if (args.new_cmd) {
      // `new` consumes its own positional name.
      args.new_name = new_cmd.get<std::string>("name");
    } else if (args.generate || args.bundle || args.run_cmd) {
      // Subcommands take root only via --root.
      args.root = opt_root.empty() ? std::string(".") : opt_root;
    } else {
      // Run / default mode: first leftover positional is the root, the rest are
      // app-level pass-through args.
      if (!remaining.empty()) {
        args.root = remaining.front();
        for (size_t i = 1; i < remaining.size(); ++i) args.positional_args.push_back(remaining[i]);
      } else {
        args.root = opt_root.empty() ? std::string(".") : opt_root;
      }
    }

    return args;
  }

  void printVersion(const char* prog) {
    coconut::println("{} {}", prog, VERSION);
  }

}  // namespace coconut
