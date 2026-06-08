#include "./comment_parser.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

/// Simple field extraction from a string key = "value" pattern.
/// Works for both JSON ("key": "value") and Lua (key = "value") formats.
/// Returns the value (without quotes) or empty string if not found.
static std::string configStringField(const std::string& text, const std::string& key) {
  // Try Lua-style: key = "value"
  {
    std::string search = key + " = \"";
    size_t pos = text.find(search);
    if (pos != std::string::npos) {
      size_t start = pos + search.size();
      size_t end = text.find('"', start);
      if (end != std::string::npos) {
        return text.substr(start, end - start);
      }
    }
  }
  // Try JSON-style: "key": "value"
  {
    std::string search = "\"" + key + "\"";
    size_t pos = text.find(search);
    if (pos == std::string::npos) return "";
    pos = text.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = text.find_first_of('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = text.find_first_of('"', pos + 1);
    if (end == std::string::npos) return "";
    return text.substr(pos + 1, end - pos - 1);
  }
}

/// Derive a Lua module path from a file path relative to the project.
/// e.g. "samples/commands/hello.lua"  → "commands.hello"
///       "src/commands/foo.lua"       → "commands.foo"
static std::string deriveModulePath(const std::string& filePath) {
  std::string stem = filePath;
  // Strip ".lua" suffix
  if (stem.size() >= 4 && stem.substr(stem.size() - 4) == ".lua") {
    stem = stem.substr(0, stem.size() - 4);
  }
  // Remove leading directory up to first '/' (e.g. "samples/", "src/")
  size_t slash = stem.find('/');
  if (slash != std::string::npos) {
    stem = stem.substr(slash + 1);
  }
  // Replace '/' with '.'
  for (auto& c : stem) {
    if (c == '/') c = '.';
  }
  return stem;
}

/// Get the output stem (filename without extension) for generated files.
static std::string outputStem(const std::string& filePath) {
  fs::path p(filePath);
  return p.stem().string();
}

static void printUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " <input.lua> [--out-dir <dir>]\n";
  std::cerr << "\n";
  std::cerr << "Parse Lua doc comments (@command, @param, @return) and generate:\n";
  std::cerr << "  <stem>.g.lua   — Lua command registration wrapper\n";
  std::cerr << "  <stem>.d.ts    — TypeScript declaration file\n";
  std::cerr << "  <stem>.g.ts    — TypeScript runtime wrappers (__coconut_call)\n";
}

int main(int argc, char** argv) {
  // ---- Parse CLI ----
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string inputPath = argv[1];
  std::string outDir = "generated";

  // Load config from coconut.config.lua (or coconut.config.json) if present.
  {
    // Try Lua config first
    std::ifstream luaCfg("coconut.config.lua");
    if (luaCfg.is_open()) {
      std::stringstream cfgBuf;
      cfgBuf << luaCfg.rdbuf();
      std::string dir = configStringField(cfgBuf.str(), "output_dir");
      if (!dir.empty()) {
        outDir = dir;
      }
    } else {
      // Fall back to JSON config
      std::ifstream cfgFile("coconut.config.json");
      if (cfgFile.is_open()) {
        std::stringstream cfgBuf;
        cfgBuf << cfgFile.rdbuf();
        std::string dir = configStringField(cfgBuf.str(), "output_dir");
        if (!dir.empty()) {
          outDir = dir;
        }
      }
    }
  }

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--out-dir" && i + 1 < argc) {
      outDir = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }

  // ---- Read input ----
  std::ifstream file(inputPath);
  if (!file.is_open()) {
    std::cerr << "Error: could not open " << inputPath << "\n";
    return 1;
  }

  std::stringstream buffer;
  std::string line;
  while (std::getline(file, line)) {
    buffer << line << "\n";
  }
  file.close();

  // ---- Parse ----
  auto commands = coconut::generator::commentsFsm(buffer.str());

  if (commands.empty()) {
    std::cerr << "No @command definitions found in " << inputPath << "\n";
    return 0;
  }

  // ---- Derive paths ----
  std::string modulePath = deriveModulePath(inputPath);
  std::string stem = outputStem(inputPath);

  // Ensure output directory exists
  fs::create_directories(outDir);

  // ---- Write Lua wrapper (.g.lua) ----
  {
    auto luaWrap = coconut::generator::generateLuaWrapper(commands, modulePath);
    fs::path outPath = fs::path(outDir) / (stem + ".g.lua");
    std::ofstream out(outPath);
    out << luaWrap;
    out.close();
    std::cout << "  wrote " << outPath << "\n";
  }

  // ---- Write TS declarations (.d.ts) ----
  {
    auto dts = coconut::generator::generateTSDefinition(commands);
    fs::path outPath = fs::path(outDir) / (stem + ".d.ts");
    std::ofstream out(outPath);
    out << dts;
    out.close();
    std::cout << "  wrote " << outPath << "\n";
  }

  // ---- Write TS wrappers (.g.ts) ----
  {
    auto wrappers = coconut::generator::generateTSWrapper(commands);
    fs::path outPath = fs::path(outDir) / (stem + ".g.ts");
    std::ofstream out(outPath);
    out << wrappers;
    out.close();
    std::cout << "  wrote " << outPath << "\n";
  }

  std::cout << "Generated " << commands.size() << " command(s) from "
            << inputPath << " → " << outDir << "/" << "\n";
  return 0;
}
