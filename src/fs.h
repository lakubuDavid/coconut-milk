#ifndef FS_H
#define FS_H

#include "config.h"
#include "error.h"

#include <expected>
#include <string>
#include <vector>

namespace coconut {
  namespace fs {

    struct Roots {
      Config*     configs = nullptr;
      std::string view_root;
      std::string asset_root;
      std::string command_root;
    };

    std::expected<Roots*, Error> create(Config* config);
    void                         destroy(Roots* roots);

    // ── File I/O ─────────────────────────────────────────────────

    /// Read entire file as text (UTF-8). Returns contents or Error.
    std::expected<std::string, Error> readText(const std::string& path);

    /// Read entire file as raw bytes.
    std::expected<std::vector<uint8_t>, Error> readBytes(const std::string& path);

    /// Write text string to file (overwrites). Returns Ok or Error.
    std::expected<void, Error> writeText(const std::string& path,
                                          const std::string& content);

    /// Write raw bytes to file (overwrites). Returns Ok or Error.
    std::expected<void, Error> writeBytes(const std::string& path,
                                           const std::vector<uint8_t>& data);

    /// Check whether a file exists.
    bool exists(const std::string& path);

    /// Resolve a relative path against the given root directory.
    /// If path is already absolute, returns it as-is.
    std::string resolve(const std::string& root,
                        const std::string& relpath);

  }  // namespace fs
}  // namespace coconut

#endif  // FS_H
