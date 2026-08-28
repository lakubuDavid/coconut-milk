/// coconut-cli — generator + scaffolder front-end.
///
/// A slim binary exposing only the command-definition generator and the
/// project scaffolder. No GUI, no runtime, no Lua — useful in CI and
/// editor tooling. Argument parsing uses p-ranav/argparse (header-only).
///
/// Usage:
///   coconut-cli generate [cmdRoot] [--out-dir D] [--watch]
///   coconut-cli new <name> [-t bare|bare-ts|vite] [-y]
///   coconut-cli help
///
/// Unlike `coconut generate`, this front-end does not parse
/// coconut.config.lua (that would pull in the Lua runtime); pass
/// --out-dir explicitly when your project overrides output_dir.

#include "generators/generate.h"
#include "new_project.h"
#include "print.h"

#include <argparse/argparse.hpp>
#include <iostream>
#include <string>

namespace {

  int cmdGenerate(int argc, char* argv[]) {
    argparse::ArgumentParser program("coconut-cli generate");

    program.add_argument("cmd_root")
        .help("command definitions root (default: commands)")
        .default_value(std::string("commands"));
    program.add_argument("-o", "--out-dir")
        .help("output directory (default: generated)")
        .default_value(std::string("generated"));
    program.add_argument("--watch").help("watch for file changes and auto-regenerate").flag();

    try {
      program.parse_args(argc, argv);
    } catch (const std::exception& err) {
      coconut::println(std::cerr, "{}", err.what());
      std::cerr << program;
      return 1;
    }

    const auto cmdRoot = program.get<std::string>("cmd_root");
    const auto outDir  = program.get<std::string>("--out-dir");
    if (program.get<bool>("--watch")) {
      return coconut::generator::runGenerateWatch(cmdRoot, outDir);
    }
    return coconut::generator::runGenerate(cmdRoot, outDir);
  }

  int cmdNew(int argc, char* argv[]) {
    argparse::ArgumentParser program("coconut-cli new");

    program.add_argument("name").help("project name to scaffold");
    program.add_argument("-t", "--template")
        .help("template: bare (default), bare-ts, vite")
        .default_value(std::string("bare"));
    program.add_argument("-y", "--yes").help("skip prompts, use defaults").flag();

    try {
      program.parse_args(argc, argv);
    } catch (const std::exception& err) {
      coconut::println(std::cerr, "{}", err.what());
      std::cerr << program;
      return 1;
    }

    const auto name = program.get<std::string>("name");
    const auto tmpl = program.get<std::string>("--template");
    const bool yes  = program.get<bool>("--yes");

    std::string error;
    if (!coconut::scaffoldProject(name, tmpl, yes, error)) {
      coconut::println(std::cerr, "error: {}", error);
      return 1;
    }
    return 0;
  }

  void printUsage() {
    std::cout << "coconut-cli — generator + scaffolder for Coconut Milk apps\n\n"
                 "Usage:\n"
                 "  coconut-cli generate [cmdRoot] [--out-dir D] [--watch]\n"
                 "      Scan .lua command definitions and emit .g.lua/.g_mt.lua/.g.js/.d.ts\n"
                 "      cmdRoot defaults to 'commands'; generated output to 'generated'.\n"
                 "\n"
                 "  coconut-cli new <name> [-t bare|bare-ts|vite] [-y]\n"
                 "      Scaffold a new project interactively (-y skips prompts).\n"
                 "\n"
                 "  coconut-cli help\n"
                 "      Show this message.\n"
              << std::endl;
  }

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }
  const std::string cmd = argv[1];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    printUsage();
    return 0;
  }
  if (cmd == "generate") {
    return cmdGenerate(argc - 1, argv + 1);
  }
  if (cmd == "new") {
    if (argc < 3) {
      coconut::println(std::cerr, "error: 'new' requires a project name");
      return 1;
    }
    return cmdNew(argc - 1, argv + 1);
  }
  coconut::println(std::cerr, "unknown command: {}", cmd);
  printUsage();
  return 1;
}
