#include "fs.h"
#include "../fs.h"  // C++ fs namespace (coconut::fs)
#include "debug.h"

#include <format>

namespace coconut::modules {

  void init_fs(sol::state& lua, ThreadKind kind) {
    (void)kind;  // thread-safe

    sol::table coconut = lua["coconut"].get_or_create<sol::table>();
    sol::table fs_mod  = lua.create_table();

    fs_mod.set_function("readText", [](const std::string& path) -> std::string {
      auto result = coconut::fs::readText(path);
      if (result)
        return std::move(*result);
      debug::warn(std::format("fs.readText: {} ({})", result.error().message, path));
      return {};
    });

    fs_mod.set_function("readBytes", [](const std::string& path) -> std::string {
      auto result = coconut::fs::readBytes(path);
      if (result) {
        auto& vec = *result;
        return std::string(reinterpret_cast<const char*>(vec.data()), vec.size());
      }
      debug::warn(std::format("fs.readBytes: {} ({})", result.error().message, path));
      return {};
    });

    fs_mod.set_function(
        "writeText",
        [](const std::string& path, const std::string& content) -> bool {
          auto result = coconut::fs::writeText(path, content);
          if (result)
            return true;
          debug::warn(std::format("fs.writeText: {} ({})", result.error().message, path));
          return false;
        }
    );

    fs_mod.set_function("writeBytes", [](const std::string& path, const std::string& data) -> bool {
      std::vector<uint8_t> vec(data.begin(), data.end());
      auto                 result = coconut::fs::writeBytes(path, vec);
      if (result)
        return true;
      debug::warn(std::format("fs.writeBytes: {} ({})", result.error().message, path));
      return false;
    });

    fs_mod.set_function("exists", [](const std::string& path) -> bool {
      return coconut::fs::exists(path);
    });

    fs_mod.set_function(
        "resolve",
        [](const std::string& root, const std::string& relpath) -> std::string {
          return coconut::fs::resolve(root, relpath);
        }
    );

    fs_mod.set_function("listDir", [](const std::string& path, sol::this_state s) -> sol::table {
      sol::state_view lv(s);
      sol::table      results = lv.create_table();
      auto            entries = coconut::fs::listDir(path);
      if (!entries)
        return results;
      for (size_t i = 0; i < entries->size(); ++i) {
        sol::table entry = lv.create_table();
        entry["name"]    = (*entries)[i].name;
        entry["path"]    = (*entries)[i].path;
        entry["is_dir"]  = (*entries)[i].is_dir;
        results[i + 1]   = entry;
      }
      return results;
    });

    coconut["fs"] = fs_mod;
  }

}  // namespace coconut::modules
