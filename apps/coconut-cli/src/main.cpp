/// coconut-cli — generator + scaffolder front-end.
///
/// A slim binary exposing only the command-definition generator and the
/// project scaffolder from the coconut app tree. No GUI, no runtime, no
/// external packages — useful in CI and editor tooling.
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

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void printUsage() {
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

static int cmdGenerate(const std::vector<std::string>& args) {
  std::string cmdRoot = "commands";
  std::string outDir  = "generated";
  bool        watch   = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--watch") {
      watch = true;
    } else if (a == "--out-dir" && i + 1 < args.size()) {
      outDir = args[++i];
    } else if (!a.empty() && a[0] != '-') {
      cmdRoot = a;  // positional: command root override
    } else {
      coconut::println(std::cerr, "unknown option: {}", a);
      return 1;
    }
  }

  if (watch) {
    return coconut::generator::runGenerateWatch(cmdRoot, outDir);
  }
  return coconut::generator::runGenerate(cmdRoot, outDir);
}

static int cmdNew(const std::vector<std::string>& args) {
  std::string name;
  std::string template_name;
  bool        yes = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if ((a == "-t" || a == "--template") && i + 1 < args.size()) {
      template_name = args[++i];
    } else if (a == "-y" || a == "--yes") {
      yes = true;
    } else if (!a.empty() && a[0] != '-' && name.empty()) {
      name = a;
    } else {
      coconut::println(std::cerr, "unknown argument: {}", a);
      return 1;
    }
  }

  std::string error;
  if (!coconut::scaffoldProject(name, template_name, yes, error)) {
    coconut::println(std::cerr, "error: {}", error);
    return 1;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  std::vector<std::string> args(argv + (argc > 1 ? 1 : argc), argv + argc);

  if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
    printUsage();
    return args.empty() ? 1 : 0;
  }

  const std::string        cmd = args[0];
  std::vector<std::string> rest(args.begin() + 1, args.end());

  if (cmd == "generate") {
    return cmdGenerate(rest);
  }
  if (cmd == "new") {
    return cmdNew(rest);
  }

  coconut::println(std::cerr, "unknown command: {}", cmd);
  printUsage();
  return 1;
}
