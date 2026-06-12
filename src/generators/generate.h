#pragma once

#include <string>

namespace coconut::generator {

/// Run the command generation pass.
///
/// Scans all .lua files in the command root for @command annotations,
/// generates type-safe wrappers (.g.lua, .g.js, .d.ts) and an aggregated
/// commands.d.ts with a union type of all command names.
///
/// @param cmdRoot   Directory containing command .lua files (default: "commands")
/// @param outDir    Output directory for generated files (default: "generated")
/// @return 0 on success, 1 on failure
int runGenerate(const std::string& cmdRoot = "commands",
                const std::string& outDir = "generated");

} // namespace coconut::generator
