#ifndef COCONUT_MODULES_DIALOG_H
#define COCONUT_MODULES_DIALOG_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

/// Register coconut.dialog.message / .open / .save.
/// On Background, registers stubs that return error messages.
void init_dialog(sol::state& lua, ThreadKind kind);

} // namespace coconut::modules

#endif
