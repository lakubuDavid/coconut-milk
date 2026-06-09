#include "fs.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace coconut::fs {

std::expected<Roots*, Error> create(Config* config) {
  return new Roots{.configs = config,
                   .view_root = config != nullptr ? config->view_root : std::string{},
                   .asset_root = config != nullptr ? config->asset_root : std::string{},
                   .command_root = config != nullptr ? config->command_root : std::string{}};
}

void destroy(Roots* roots) {
  delete roots;
}

// ── File I/O ───────────────────────────────────────────────────────

std::expected<std::string, Error> readText(const std::string& path) {
  try {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
      return std::unexpected(Error{
          .code = ErrorCode::MissingFile,
          .message = "could not open file: " + path});
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
  } catch (const std::exception& e) {
    return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::string("readText error: ") + e.what(),
        .details = path});
  }
}

std::expected<std::vector<uint8_t>, Error> readBytes(const std::string& path) {
  try {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      return std::unexpected(Error{
          .code = ErrorCode::MissingFile,
          .message = "could not open file: " + path});
    }
    file.seekg(0, std::ios::end);
    auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(size);
    file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    if (!file) {
      return std::unexpected(Error{
          .code = ErrorCode::IoError,
          .message = "readBytes: short read: " + path});
    }
    return buf;
  } catch (const std::exception& e) {
    return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::string("readBytes error: ") + e.what(),
        .details = path});
  }
}

std::expected<void, Error> writeText(const std::string& path,
                                      const std::string& content) {
  try {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
      return std::unexpected(Error{
          .code = ErrorCode::IoError,
          .message = "could not open file for writing: " + path});
    }
    file << content;
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::string("writeText error: ") + e.what(),
        .details = path});
  }
}

std::expected<void, Error> writeBytes(const std::string& path,
                                       const std::vector<uint8_t>& data) {
  try {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      return std::unexpected(Error{
          .code = ErrorCode::IoError,
          .message = "could not open file for writing: " + path});
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::string("writeBytes error: ") + e.what(),
        .details = path});
  }
}

bool exists(const std::string& path) {
  return std::filesystem::exists(path);
}

std::string resolve(const std::string& root, const std::string& relpath) {
  auto p = std::filesystem::path(relpath);
  if (p.is_absolute()) {
    return p.lexically_normal().string();
  }
  auto r = std::filesystem::path(root);
  return (r / p).lexically_normal().string();
}

}  // namespace coconut::fs
