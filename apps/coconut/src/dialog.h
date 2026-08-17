#ifndef COCONUT_DIALOG_H
#define COCONUT_DIALOG_H

#include <string>
#include <vector>

namespace coconut {
namespace dialog {

/// Result returned from a dialog interaction.
struct Result {
  bool confirmed = false;       /// true if the user clicked OK/Yes
  std::string path;             /// selected file path (single-selection)
  std::vector<std::string> paths; /// all selected paths (multi-selection)
  bool is_dir = false;          /// true if selected path is a directory
};

/// File filter description.
struct Filter {
  std::string name;             /// display name, e.g. "Images"
  std::vector<std::string> patterns; /// e.g. {"*.png","*.jpg"}
};

/// --- Public API (platform dispatchers) ---

/// Show a native message box.
/// \param title  Window title
/// \param message Body text
/// \param kind   "info", "warn", "error", or "question"
/// \returns result.confirmed = true if the user pressed OK/Yes
Result messageBox(const std::string& title,
                  const std::string& message,
                  const std::string& kind = "info");

/// Show a native "Open File" / "Open" dialog.
/// \param chooseDir  If true, the user can also pick directories.
Result openFile(const std::string& title,
                const std::vector<Filter>& filters = {},
                bool multi = false,
                bool chooseDir = false);

/// Show a native "Save File" dialog.
Result saveFile(const std::string& title,
                const std::string& defaultName = "",
                const std::vector<Filter>& filters = {});

} // namespace dialog
} // namespace coconut

#endif // COCONUT_DIALOG_H
