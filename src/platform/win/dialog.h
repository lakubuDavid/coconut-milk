#ifndef COCONUT_PLATFORM_WIN_DIALOG_H
#define COCONUT_PLATFORM_WIN_DIALOG_H

#include "../../dialog.h"

namespace coconut {
namespace dialog {

Result platformMessageBox(const std::string& title,
                          const std::string& message,
                          const std::string& kind);

Result platformOpenFile(const std::string& title,
                        const std::vector<Filter>& filters,
                        bool multi);

Result platformSaveFile(const std::string& title,
                        const std::string& defaultName,
                        const std::vector<Filter>& filters);

} // namespace dialog
} // namespace coconut

#endif // COCONUT_PLATFORM_WIN_DIALOG_H
