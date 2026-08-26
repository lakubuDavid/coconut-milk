#ifndef COCONUT_PLATFORM_DARWIN_DIALOG_H
#define COCONUT_PLATFORM_DARWIN_DIALOG_H

#include "../../dialog.h"

#include <expected>

namespace coconut {
  namespace dialog {

    std::expected<Result, Error> platformMessageBox(
        const std::string& title, const std::string& message, const std::string& kind
    );

    std::expected<Result, Error> platformOpenFile(
        const std::string&         title,
        const std::vector<Filter>& filters,
        bool                       multi,
        bool                       chooseDir = false
    );

    std::expected<Result, Error> platformSaveFile(
        const std::string& title, const std::string& defaultName, const std::vector<Filter>& filters
    );

  }  // namespace dialog
}  // namespace coconut

#endif  // COCONUT_PLATFORM_DARWIN_DIALOG_H
