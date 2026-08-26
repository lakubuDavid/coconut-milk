#include "dialog.h"
#include <cstddef>
#include <sol/state.hpp>
#include <sol/table.hpp>
#include <sol/variadic_args.hpp>
#include <string>
#include <vector>
#include "../dialog.h"  // C++ dialog namespace
#include "forward.h"
#include "modules/thread_kind.h"

namespace coconut::modules {

  void init_dialog(sol::state& lua, ThreadKind kind) {
    sol::table coconut    = lua["coconut"].get_or_create<sol::table>();
    sol::table dialog_mod = lua.create_table();

    if (kind == ThreadKind::Main) {
      dialog_mod.set_function("message", [](sol::variadic_args va) -> sol::table {
        sol::state_view lv    = va.lua_state();
        std::string     title = "Message";
        std::string     message;
        std::string     kind_str = "info";
        if (va.size() >= 1 && va[0].is<std::string>())
          message = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<std::string>())
          title = va[1].as<std::string>();
        if (va.size() >= 3 && va[2].is<std::string>())
          kind_str = va[2].as<std::string>();
        auto       exp = coconut::dialog::messageBox(title, message, kind_str);
        sol::table t   = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        t["confirmed"] = exp->confirmed;
        return t;
      });

      dialog_mod.set_function("open", [](sol::variadic_args va) -> sol::table {
        sol::state_view                      lv        = va.lua_state();
        std::string                          title     = "Open File";
        bool                                 multi     = false;
        bool                                 chooseDir = false;
        std::vector<coconut::dialog::Filter> filters;
        if (va.size() >= 1 && va[0].is<std::string>())
          title = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<bool>())
          multi = va[1].as<bool>();
        if (va.size() >= 3 && va[2].is<bool>())
          chooseDir = va[2].as<bool>();
        auto       exp = coconut::dialog::openFile(title, filters, multi, chooseDir);
        sol::table t   = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        const auto& r    = *exp;
        t["confirmed"]   = r.confirmed;
        t["path"]        = r.path;
        t["is_dir"]      = r.is_dir;
        sol::table paths = lv.create_table();
        for (size_t i = 0; i < r.paths.size(); ++i) paths[i + 1] = r.paths[i];
        t["paths"] = paths;
        return t;
      });

      dialog_mod.set_function("save", [](sol::variadic_args va) -> sol::table {
        sol::state_view lv    = va.lua_state();
        std::string     title = "Save File";
        std::string     defaultName;
        if (va.size() >= 1 && va[0].is<std::string>())
          title = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<std::string>())
          defaultName = va[1].as<std::string>();
        auto       exp = coconut::dialog::saveFile(title, defaultName);
        sol::table t   = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        t["confirmed"] = exp->confirmed;
        t["path"]      = exp->path;
        return t;
      });
    } else {
      // Background — forward onto the main run loop (dispatchPost) and
      // block until the native dialog completes. Result tables are built
      // on the calling state.
      dialog_mod.set_function("message", [](sol::variadic_args va) -> sol::table {
        sol::state_view lv(va.lua_state());
        std::string     title = "Message";
        std::string     message;
        std::string     kind_str = "info";
        if (va.size() >= 1 && va[0].is<std::string>())
          message = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<std::string>())
          title = va[1].as<std::string>();
        if (va.size() >= 3 && va[2].is<std::string>())
          kind_str = va[2].as<std::string>();
        auto exp =
            forwardToMain([&] { return coconut::dialog::messageBox(title, message, kind_str); });
        sol::table t = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        t["confirmed"] = exp->confirmed;
        return t;
      });

      dialog_mod.set_function("open", [](sol::variadic_args va) -> sol::table {
        sol::state_view lv(va.lua_state());
        std::string     title     = "Open File";
        bool            multi     = false;
        bool            chooseDir = false;
        if (va.size() >= 1 && va[0].is<std::string>())
          title = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<bool>())
          multi = va[1].as<bool>();
        if (va.size() >= 3 && va[2].is<bool>())
          chooseDir = va[2].as<bool>();
        auto exp =
            forwardToMain([&] { return coconut::dialog::openFile(title, {}, multi, chooseDir); });
        sol::table t = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        const auto& r    = *exp;
        t["confirmed"]   = r.confirmed;
        t["path"]        = r.path;
        t["is_dir"]      = r.is_dir;
        sol::table paths = lv.create_table();
        for (size_t i = 0; i < r.paths.size(); ++i) paths[i + 1] = r.paths[i];
        t["paths"] = paths;
        return t;
      });

      dialog_mod.set_function("save", [](sol::variadic_args va) -> sol::table {
        sol::state_view lv(va.lua_state());
        std::string     title = "Save File";
        std::string     defaultName;
        if (va.size() >= 1 && va[0].is<std::string>())
          title = va[0].as<std::string>();
        if (va.size() >= 2 && va[1].is<std::string>())
          defaultName = va[1].as<std::string>();
        auto exp     = forwardToMain([&] { return coconut::dialog::saveFile(title, defaultName); });
        sol::table t = lv.create_table();
        if (!exp) {
          t["confirmed"] = false;
          t["error"]     = exp.error().message;
          return t;
        }
        t["confirmed"] = exp->confirmed;
        t["path"]      = exp->path;
        return t;
      });
    }

    coconut["dialog"] = dialog_mod;
  }

}  // namespace coconut::modules
