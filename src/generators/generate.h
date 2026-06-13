#pragma once

#include <string>

namespace coconut::generator {

/// Result of a single file generation.
struct FileGenResult {
  std::string file;     ///< Source command file name
  int        commands;  ///< Number of @command annotations found
  bool       skipped;   ///< True if file was skipped (no changes)
  bool       ok;        ///< True if generation succeeded (or skipped)
};

/// Run the command generation pass.
///
/// Scans all .lua files in the command root for @command annotations,
/// generates type-safe wrappers (.g.lua, .g.js, .d.ts) and an aggregated
/// commands.d.ts with a union type of all command names.
///
/// Outputs progress to stdout.
///
/// @param cmdRoot   Directory containing command .lua files (default: "commands")
/// @param outDir    Output directory for generated files (default: "generated")
/// @return 0 on success (or no work to do), 1 on failure
int runGenerate(const std::string& cmdRoot = "commands",
                const std::string& outDir = "generated");

} // namespace coconut::generator
