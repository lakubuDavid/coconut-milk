#include "store.h"
#include "test.h"

COCONUT_TEST(store, create_and_destroy) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result.value() != nullptr);
  coconut::store::destroy(result.value());
}

COCONUT_TEST(store, set_and_get) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  coconut::store::set(store, "key1", "value1");
  auto getResult = coconut::store::get(store, "key1");
  COCONUT_REQUIRE(getResult.has_value());
  COCONUT_REQUIRE_EQ(getResult.value(), "value1");

  coconut::store::destroy(store);
}

COCONUT_TEST(store, get_nonexistent_key) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  auto getResult = coconut::store::get(store, "nonexistent");
  COCONUT_REQUIRE(!getResult.has_value());
  COCONUT_REQUIRE_EQ(getResult.error().code, coconut::ErrorCode::NotFound);

  coconut::store::destroy(store);
}

COCONUT_TEST(store, has_key) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  COCONUT_REQUIRE(!coconut::store::has(store, "key1"));
  
  coconut::store::set(store, "key1", "value1");
  COCONUT_REQUIRE(coconut::store::has(store, "key1"));
  COCONUT_REQUIRE(!coconut::store::has(store, "key2"));

  coconut::store::destroy(store);
}

COCONUT_TEST(store, remove_key) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  coconut::store::set(store, "key1", "value1");
  COCONUT_REQUIRE(coconut::store::has(store, "key1"));

  coconut::store::remove(store, "key1");
  COCONUT_REQUIRE(!coconut::store::has(store, "key1"));

  coconut::store::destroy(store);
}

COCONUT_TEST(store, clear_all) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  coconut::store::set(store, "key1", "value1");
  coconut::store::set(store, "key2", "value2");
  coconut::store::set(store, "key3", "value3");

  auto keys = coconut::store::keys(store);
  COCONUT_REQUIRE_EQ(keys.size(), 3);

  coconut::store::clear(store);
  
  keys = coconut::store::keys(store);
  COCONUT_REQUIRE_EQ(keys.size(), 0);

  coconut::store::destroy(store);
}

COCONUT_TEST(store, keys_list) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  coconut::store::set(store, "alpha", "1");
  coconut::store::set(store, "beta", "2");
  coconut::store::set(store, "gamma", "3");

  auto keys = coconut::store::keys(store);
  COCONUT_REQUIRE_EQ(keys.size(), 3);

  // Check all keys are present (order not guaranteed in unordered_map)
  bool has_alpha = false, has_beta = false, has_gamma = false;
  for (const auto& key : keys) {
    if (key == "alpha") has_alpha = true;
    if (key == "beta") has_beta = true;
    if (key == "gamma") has_gamma = true;
  }
  COCONUT_REQUIRE(has_alpha);
  COCONUT_REQUIRE(has_beta);
  COCONUT_REQUIRE(has_gamma);

  coconut::store::destroy(store);
}

COCONUT_TEST(store, overwrite_value) {
  auto result = coconut::store::create();
  COCONUT_REQUIRE(result.has_value());
  auto* store = result.value();

  coconut::store::set(store, "key1", "value1");
  auto r1 = coconut::store::get(store, "key1");
  COCONUT_REQUIRE(r1.has_value());
  COCONUT_REQUIRE_EQ(r1.value(), "value1");

  coconut::store::set(store, "key1", "value2");
  auto r2 = coconut::store::get(store, "key1");
  COCONUT_REQUIRE(r2.has_value());
  COCONUT_REQUIRE_EQ(r2.value(), "value2");

  auto keys = coconut::store::keys(store);
  COCONUT_REQUIRE_EQ(keys.size(), 1);

  coconut::store::destroy(store);
}
