#include "./command_definition.hpp"
#include "./generate.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ── Builtin command definitions ─────────────────────────────────────────
// These commands are registered by the runtime in _registerBuiltinCommands
// and are always available.  The generator includes them in commands.d.ts
// so that coconut.call() is typed for builtins too.
//
// paramsType / returnType are TypeScript type strings (not Lua).
// These stay in sync with src/lua_runtime.cpp.

static constexpr struct {
  const char* name;
  const char* paramsType;   // TS type for the payload object
  const char* returnType;   // TS type for the return value
} kBuiltinCommands[] = {
  // ── Core ──
  { "ping",             "{}",                           "string" },
  { "getViews",         "{}",                           "string[]" },

  // ── Window control (internal) ──
  { "__coconutWindowCtl",
    "{ cmd: string; x?: number; y?: number; w?: number; h?: number }",
    "{ ok: boolean }" },

  // ── Clipboard ──
  { "clipboard_read",   "{}",                           "string" },
  { "clipboard_write",  "{ text: string }",             "boolean" },

  // ── System ──
  { "openUrl",          "{ url: string }",              "boolean" },
  { "notify",           "{ title: string; body: string }", "boolean" },

  // ── Dialogs ──
  { "dialog_message",
    "{ message?: string; title?: string; kind?: string }",
    "{ confirmed: boolean }" },
  { "dialog_open",
    "{ title?: string; multi?: boolean; chooseDir?: boolean }",
    "{ confirmed: boolean; path: string; paths: string[] }" },
  { "dialog_save",
    "{ title?: string; defaultName?: string }",
    "{ confirmed: boolean; path: string }" },

  // ── Filesystem ──
  { "fs_read_text",     "{ path: string }",
    "{ ok: boolean; data?: string; error?: string }" },
  { "fs_exists",        "{ path: string }",
    "{ ok: boolean; exists?: boolean; error?: string }" },
  { "fs_write_text",    "{ path: string; content: string }",
    "{ ok: boolean; error?: string }" },
  { "fs_resolve",       "{ root: string; relpath: string }",
    "{ ok: boolean; data?: string; error?: string }" },
  { "fs_list_dir",      "{ path: string }",
    "{ ok: boolean; data?: { name: string; path: string; is_dir: boolean }[]; error?: string }" },
};

static constexpr size_t kNumBuiltins = sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0]);

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
  fs::path p(filePath);
  std::string stem = p.stem().string();
  // Include the directory path as dotted module prefix so that
  //   require("commands.editor")
  // resolves correctly via the default ./?.lua pattern.
  // Only apply when the relative path is simple (single dir level).
  // Skip for absolute paths or deeply nested ones.
  if (p.has_parent_path()) {
    fs::path parent = p.parent_path();
    std::string parentStr = parent.string();
    if (!parentStr.empty() && parentStr != "." &&
        parentStr.find('/') == std::string::npos &&
        parentStr.find('\\') == std::string::npos) {
      return parentStr + "." + stem;
    }
  }
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

/// Build a TS object type string from a CommandDefinition's parameters.
/// If the def has one parameter with a table type, use that type directly.
/// Otherwise synthesize { field1: type1; ... } from individual params.
static std::string buildParamsType(const coconut::generator::CommandDefinition& def) {
  if (def.parameters.empty())
    return "{}";
  if (def.parameters.size() == 1) {
    std::string t = coconut::generator::formatTypeOrPassthrough(def.parameters[0].type,
                                                                 coconut::generator::formatTypeTS);
    if (!t.empty() && t != "any")
      return t;
    // fall through to build from param name
  }
  // Multiple params or single param with no usable type:
  // synthesize { name1: type1; name2: type2; ... }
  std::string out = "{ ";
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    if (i > 0) out += "; ";
    out += def.parameters[i].name;
    out += ": ";
    out += coconut::generator::formatTypeOrPassthrough(def.parameters[i].type,
                                                       coconut::generator::formatTypeTS);
  }
  out += " }";
  return out;
}

/// Build a TS return type string from a CommandDefinition.
static std::string buildReturnType(const coconut::generator::CommandDefinition& def) {
  if (def.returnTypes.empty())
    return "any";
  return coconut::generator::formatTypeOrPassthrough(def.returnTypes,
                                                     coconut::generator::formatTypeTS);
}

/// Emit the builtin + user command type maps into the aggregated commands.d.ts.
static void writeAggregatedDTS(std::ostream& agg,
                               const std::vector<std::string>& userNames,
                               const std::vector<coconut::generator::CommandDefinition>& userDefs) {
  agg << "// Auto-generated by coconut-milk generator. Do not edit.\n";
  agg << "// Aggregates builtin + @command names from commands/*.lua.\n";
  agg << "// Included via /// <reference> in scripts/coconut.d.ts.\n";
  agg << "\n";

  // ── Builtin param type map ──
  agg << "type BuiltinCommandParams = {\n";
  for (size_t i = 0; i < kNumBuiltins; ++i)
    agg << "  \"" << kBuiltinCommands[i].name << "\": " << kBuiltinCommands[i].paramsType << ";\n";
  agg << "};\n\n";

  // ── Builtin return type map ──
  agg << "type BuiltinCommandReturns = {\n";
  for (size_t i = 0; i < kNumBuiltins; ++i)
    agg << "  \"" << kBuiltinCommands[i].name << "\": " << kBuiltinCommands[i].returnType << ";\n";
  agg << "};\n\n";

  // ── Builtin name union ──
  agg << "type BuiltinCommandName = keyof BuiltinCommandParams;\n\n";

  // ── User command type map (intersection with builtins) ──
  // Deduplicate by name (last definition wins for conflicts).
  std::map<std::string, const coconut::generator::CommandDefinition*> uniqueByName;
  for (const auto& d : userDefs)
    uniqueByName[d.name] = &d;

  if (uniqueByName.empty()) {
    // No user commands — the *Command* aliases just mirror the builtin types
    agg << "type CoconutCommandName = BuiltinCommandName;\n";
    agg << "type CoconutCommandParams = BuiltinCommandParams;\n";
    agg << "type CoconutCommandReturns = BuiltinCommandReturns;\n";
  } else {
    // User commands exist — emit intersection + extended union
    agg << "type CoconutCommandParams = BuiltinCommandParams & {\n";
    for (const auto& [name, def] : uniqueByName)
      agg << "  \"" << name << "\": " << buildParamsType(*def) << ";\n";
    agg << "};\n\n";

    agg << "type CoconutCommandReturns = BuiltinCommandReturns & {\n";
    for (const auto& [name, def] : uniqueByName)
      agg << "  \"" << name << "\": " << buildReturnType(*def) << ";\n";
    agg << "};\n\n";

    agg << "type CoconutCommandName = BuiltinCommandName";
    for (const auto& name : userNames)
      agg << " | \"" << name << "\"";
    agg << ";\n";
  }
  agg << "\n";
}

/// Process a single .lua command file and generate output files.
static bool processCommandFile(const fs::path& inputPath,
                               const std::string& outDir,
                               std::vector<std::string>& allNames,
                               std::vector<coconut::generator::CommandDefinition>& allDefs,
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
    // Still collect command names + defs for the aggregate file
    for (const auto& cmd : commands) {
      bool found = false;
      for (const auto& n : allNames)
        if (n == cmd.name) { found = true; break; }
      if (!found) {
        allNames.push_back(cmd.name);
        allDefs.push_back(cmd);
      }
    }
    return true;
  }

  fs::create_directories(outDir);

  // Collect command names + defs for the aggregate file
  for (const auto& cmd : commands) {
    bool found = false;
    for (const auto& n : allNames)
      if (n == cmd.name) { found = true; break; }
    if (!found) {
      allNames.push_back(cmd.name);
      allDefs.push_back(cmd);
    }
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
  std::vector<CommandDefinition> allDefs;
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
    if (processCommandFile(entry.path(), outDir, allNames, allDefs, skipped)) {
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

  // Count total commands (includes builtins)
  totalCommands = static_cast<int>(allNames.size());

  // Write aggregated commands.d.ts (always — includes builtins even when no user commands)
  fs::create_directories(outDir);
  {
    std::ofstream agg(fs::path(outDir) / "commands.d.ts");
    writeAggregatedDTS(agg, allNames, allDefs);
  }

  // Report results
  if (totalCommands == 0) {
    std::println("generate: no @command annotations found in {}/*.lua", cmdRoot);
    std::println("  (add ---@command above a Lua function to generate wrappers)");
    std::println("  (builtin command types still written to commands.d.ts)");
    std::fflush(stdout);
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
  std::fflush(stdout);

  return filesError > 0 ? 1 : 0;
}

int runGenerateWatch(const std::string& cmdRoot, const std::string& outDir) {
  fs::path cmdDir = cmdRoot;
  if (!fs::is_directory(cmdDir)) {
    std::println("generate: command root '{}' not found", cmdRoot);
    return 1;
  }

  std::println("generate: watching '{}' for changes... (Ctrl+C to stop)", cmdRoot);
  std::fflush(stdout);

  // Run once immediately
  runGenerate(cmdRoot, outDir);
  std::fflush(stdout);
  std::println("");

  // Poll for file changes every 1 second
  // Track last write time per file
  std::map<std::string, fs::file_time_type> lastTimes;

  // Seed with current file times
  for (const auto& entry : fs::directory_iterator(cmdDir)) {
    if (entry.path().extension() != ".lua") continue;
    std::string name = entry.path().filename().string();
    if (name.size() > 6 && name.substr(name.size() - 6) == ".g.lua") continue;
    std::error_code ec;
    lastTimes[name] = fs::last_write_time(entry.path(), ec);
  }

  bool running = true;
  while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    bool changed = false;
    std::error_code ec;

    // Check existing files for modifications
    for (auto& [name, last] : lastTimes) {
      auto path = cmdDir / name;
      if (!fs::exists(path, ec)) continue;
      auto current = fs::last_write_time(path, ec);
      if (ec) continue;
      if (current != last) {
        last = current;
        changed = true;
      }
    }

    // Check for new files
    for (const auto& entry : fs::directory_iterator(cmdDir, ec)) {
      if (ec) break;
      if (entry.path().extension() != ".lua") continue;
      std::string name = entry.path().filename().string();
      if (name.size() > 6 && name.substr(name.size() - 6) == ".g.lua") continue;
      if (lastTimes.find(name) == lastTimes.end()) {
        std::error_code ec2;
        lastTimes[name] = fs::last_write_time(entry.path(), ec2);
        changed = true;
      }
    }

    if (changed) {
      std::println("");
      std::fflush(stdout);
      runGenerate(cmdRoot, outDir);
      std::fflush(stdout);
      std::println("");
      std::fflush(stdout);
    }
  }

  return 0;
}

} // namespace coconut::generator
