#include "store.h"

namespace coconut {
  namespace store {

    std::expected<Store*, Error> create() {
      return new Store();
    }

    void destroy(Store* store) {
      delete store;
    }

    void set(Store* store, const std::string& key, const std::string& value) {
      if (!store) return;
      store->data[key] = value;
    }

    std::expected<std::string, Error> get(Store* store, const std::string& key) {
      if (!store) {
        return std::unexpected(Error{
            .code = ErrorCode::Internal,
            .message = "store is null",
        });
      }
      auto it = store->data.find(key);
      if (it != store->data.end()) {
        return it->second;
      }
      return std::unexpected(Error{
          .code = ErrorCode::NotFound,
          .message = "key not found: " + key,
      });
    }

    bool has(Store* store, const std::string& key) {
      if (!store) return false;
      return store->data.find(key) != store->data.end();
    }

    void remove(Store* store, const std::string& key) {
      if (!store) return;
      store->data.erase(key);
    }

    void clear(Store* store) {
      if (!store) return;
      store->data.clear();
    }

    std::vector<std::string> keys(Store* store) {
      std::vector<std::string> result;
      if (!store) return result;
      
      result.reserve(store->data.size());
      for (const auto& pair : store->data) {
        result.push_back(pair.first);
      }
      return result;
    }

  }  // namespace store
}  // namespace coconut
