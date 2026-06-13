#include "./command_definition.hpp"
#include "./generate.h"

#include <filesystem>
#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/// Simple field extraction from a string key = "value" pattern.
/// Works for both JSON ("key": "value") and Lua (key = "value") formats.
static std::string configStringField(const std::string& text,
                                     const std::string& key) {
  // Try Lua-style: key = "value"
  {
    std::string search = key + " = \"";
    size_t pos = text.find(search);
    if (pos != std::string::npos) {
      size_t start = pos + search.size();
      size_t end = text.find('"', start);
      if (end != std::string::npos)
        return text.substr(start, end - start);
    }
  }
  // Try JSON-style: "key": "value"
  {
    std::string search = "\"" + key + "\"";
    size_t pos = text.find(search);
    if (pos == std::string::npos)
      return "";
    pos = text.find(':', pos + search.size());
    if (pos == std::string::npos)
      return "";
    pos = text.find_first_of('"', pos);
    if (pos == std::string::npos)
      return "";
    size_t end = text.find_first_of('"', pos + 1);
    if (end == std::string::npos)
      return "";
    return text.substr(pos + 1, end - pos - 1);
  }
}

static std::string deriveModulePath(const std::string& filePath) {
  std::string stem = fs::path(filePath).stem().string();
  return stem;
}

static std::string outputStem(const std::string& filePath) {
  return fs::path(filePath).stem().string();
}

/// Check whether the source file is newer than all generated outputs.
/// If any output is missing or older, return true (needs regeneration).
static bool needsRegeneration(const fs::path& srcPath,
                              const std::string& outDir,
                              const std::string& stem) {
  std::error_code ec;
  auto srcTime = fs::last_write_time(srcPath, ec);
  if (ec) return true;  // can't read source time, regenerate to be safe

  auto check = [&](const std::string& ext) -> bool {
    auto outPath = fs::path(outDir) / (stem + ext);
    if (!fs::exists(outPath, ec)) return true;
    auto outTime = fs::last_write_time(outPath, ec);
    if (ec) return true;
    return outTime < srcTime;  // output is older than source
  };

  return check(".g.lua") || check(".d.ts") || check(".g.js");
}

/// Process a single .lua command file and generate output files.
static bool processCommandFile(const fs::path& inputPath,
                               const std::string& outDir,
                               std::vector<std::string>& allNames,
                               bool& skipped) {
  skipped = false;

  std::ifstream file(inputPath);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  std::string line;
  while (std::getline(file, line))
    buffer << line << "\n";
  file.close();

  auto commands = coconut::generator::commentsFsm(buffer.str());
  if (commands.empty()) {
    return true; // not an error
  }

  std::string modulePath = deriveModulePath(inputPath.string());
  std::string stem = outputStem(inputPath.string());

  // Mtime check: skip if source is older than all generated outputs
  if (!needsRegeneration(inputPath, outDir, stem)) {
    skipped = true;
    // Still collect command names for the aggregate file
    for (const auto& cmd : commands) {
      bool found = false;
      for (const auto& n : allNames)
        if (n == cmd.name) { found = true; break; }
      if (!found) allNames.push_back(cmd.name);
    }
    return true;
  }

  fs::create_directories(outDir);

  // Collect command names for the aggregate file
  for (const auto& cmd : commands) {
    bool found = false;
    for (const auto& n : allNames)
      if (n == cmd.name) { found = true; break; }
    if (!found) allNames.push_back(cmd.name);
  }

  // .g.lua
  {
    auto luaWrap = coconut::generator::generateLuaWrapper(commands, modulePath);
    std::ofstream out(fs::path(outDir) / (stem + ".g.lua"));
    out << luaWrap;
  }

  // .d.ts
  {
    auto dts = coconut::generator::generateTSDefinition(commands);
    std::ofstream out(fs::path(outDir) / (stem + ".d.ts"));
    out << dts;
  }

  // .g.js
  {
    auto wrappers = coconut::generator::generateJSWrapper(commands);
    std::ofstream out(fs::path(outDir) / (stem + ".g.js"));
    out << wrappers;
  }

  return true;
}

namespace coconut::generator {

int runGenerate(const std::string& cmdRoot, const std::string& outDir) {
  std::vector<std::string> allNames;
  int totalCommands = 0;
  int filesGenerated = 0;
  int filesSkipped = 0;
  int filesError = 0;

  // Resolve the command root directory
  fs::path cmdDir = cmdRoot;
  if (!fs::is_directory(cmdDir)) {
    std::println("generate: command root '{}' not found", cmdRoot);
    return 1;
  }

  // Process each .lua file in the command root
  for (const auto& entry : fs::directory_iterator(cmdDir)) {
    if (entry.path().extension() != ".lua") continue;
    std::string name = entry.path().filename().string();
    // Skip generated files (.g.lua)
    if (name.size() > 6 && name.substr(name.size() - 6) == ".g.lua") continue;

    bool skipped = false;
    if (processCommandFile(entry.path(), outDir, allNames, skipped)) {
      if (skipped) {
        filesSkipped++;
      } else {
        filesGenerated++;
      }
    } else {
      std::println("  error: failed to process '{}'", name);
      filesError++;
    }
  }

  // Count total commands
  totalCommands = static_cast<int>(allNames.size());

  // Write aggregated commands.d.ts (always, to keep it in sync)
  if (!allNames.empty()) {
    fs::create_directories(outDir);
    std::ofstream agg(fs::path(outDir) / "commands.d.ts");
    agg << "// Auto-generated by coconut-milk generator. Do not edit.\n";
    agg << "// Aggregates all @command names from commands/*.lua.\n";
    agg << "// Included via /// <reference> in scripts/coconut.d.ts.\n";
    agg << "\n";
    agg << "type CoconutCommandName =";
    for (size_t i = 0; i < allNames.size(); ++i) {
      agg << (i == 0 ? " " : " | ") << "\"" << allNames[i] << "\"";
    }
    agg << ";\n";
  }

  // Report results
  if (totalCommands == 0) {
    std::println("generate: no @command annotations found in {}/*.lua", cmdRoot);
    std::println("  (add ---@command above a Lua function to generate wrappers)");
    return 0;  // Not an error — project may not use commands yet
  }

  std::string summary;
  if (filesGenerated > 0) {
    summary += std::to_string(filesGenerated) + " file(s) generated";
  }
  if (filesSkipped > 0) {
    if (!summary.empty()) summary += ", ";
    summary += std::to_string(filesSkipped) + " file(s) up-to-date (skipped)";
  }
  if (filesError > 0) {
    if (!summary.empty()) summary += ", ";
    summary += std::to_string(filesError) + " error(s)";
  }

  std::println("generate: {} command(s) in {} — {}",
               totalCommands, cmdRoot, summary);
  std::println("  output: {}/{{*.g.lua, *.d.ts, *.g.js, commands.d.ts}}", outDir);

  return filesError > 0 ? 1 : 0;
}

} // namespace coconut::generator
