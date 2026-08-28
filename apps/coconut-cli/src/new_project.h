#ifndef COCONUT_NEW_PROJECT_H
#define COCONUT_NEW_PROJECT_H

/// @file new_project.h
///
/// `coconut new <name>` — scaffold a new Coconut Milk project.
///
/// Creates the directory structure and skeleton files for a new app.

#include <string>

namespace coconut {

  /// Scaffold a new Coconut Milk project.
  ///
  /// @param name        Project directory name / app name (empty = script prompts)
  /// @param template_name  Template: "bare", "bare-ts", "vite" (empty = script prompts)
  /// @param yes         Skip all prompts (pass --yes to the script)
  /// @param error_out   Filled with error message on failure
  /// @return true on success
  bool scaffoldProject(
      const std::string& name, const std::string& template_name, bool yes, std::string& error_out
  );

}  // namespace coconut

#endif  // COCONUT_NEW_PROJECT_H
