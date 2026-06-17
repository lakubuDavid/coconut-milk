#ifndef COCONUT_STORE_H
#define COCONUT_STORE_H

#include "error.h"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace coconut {
  namespace store {

    struct Store {
      std::unordered_map<std::string, std::string> data;
    };

    std::expected<Store*, Error> create();
    void destroy(Store* store);

    void set(Store* store, const std::string& key, const std::string& value);
    std::expected<std::string, Error> get(Store* store, const std::string& key);
    bool has(Store* store, const std::string& key);
    void remove(Store* store, const std::string& key);
    void clear(Store* store);
    std::vector<std::string> keys(Store* store);

  }  // namespace store
}  // namespace coconut

#endif  // COCONUT_STORE_H
