/// Tests for bridge message envelope construction, JSON↔Lua conversion,
/// and script generation.
///
/// Failures mark still-unimplemented bridge pipeline features.

#include "bridge.h"
#include "config.h"
#include "test.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <string>

COCONUT_TEST(integration, bridge_toTable_json_object_to_lua) {
  // NOTE: toTable needs a sol::state_view. If the function isn't linked
  // or throws, this test will fail — marking the missing bridge glue.
  sol::state lua;
  lua.open_libraries();

  nlohmann::json j = {{"name", "Alice"}, {"age", 30}, {"active", true}};

  auto t = coconut::bridge::toTable(lua, j);
  COCONUT_REQUIRE(t.valid());

  std::string name = t["name"];
  int age = t["age"];
  bool active = t["active"];

  COCONUT_REQUIRE_EQ(name, std::string("Alice"));
  COCONUT_REQUIRE_EQ(age, 30);
  COCONUT_REQUIRE(active);
}

COCONUT_TEST(integration, bridge_toTable_json_array_to_lua) {
  sol::state lua;
  lua.open_libraries();

  nlohmann::json j = nlohmann::json::array({"a", "b", "c"});

  auto t = coconut::bridge::toTable(lua, j);
  COCONUT_REQUIRE(t.valid());

  // Lua arrays are 1-indexed.
  std::string v1 = t[1];
  std::string v2 = t[2];
  std::string v3 = t[3];

  COCONUT_REQUIRE_EQ(v1, std::string("a"));
  COCONUT_REQUIRE_EQ(v2, std::string("b"));
  COCONUT_REQUIRE_EQ(v3, std::string("c"));
}

COCONUT_TEST(integration, bridge_toTable_json_nested) {
  sol::state lua;
  lua.open_libraries();

  nlohmann::json j = {{"items", nlohmann::json::array({1, 2, 3})},
                       {"meta", {{"total", 3}}}};

  auto t = coconut::bridge::toTable(lua, j);
  COCONUT_REQUIRE(t.valid());

  sol::table items = t["items"];
  sol::table meta = t["meta"];

  COCONUT_REQUIRE_EQ(items[1], 1);
  COCONUT_REQUIRE_EQ(items[3], 3);
  COCONUT_REQUIRE_EQ(meta["total"], 3);
}

COCONUT_TEST(integration, bridge_toTable_string_parsing) {
  sol::state lua;
  lua.open_libraries();

  // JSON string input.
  std::string json_str = R"({"ok":true,"data":"hello"})";
  auto t = coconut::bridge::toTable(lua, json_str);
  COCONUT_REQUIRE(t.valid());
  bool ok_val = t["ok"];
  COCONUT_REQUIRE(ok_val);
  std::string data_val = t["data"];
  COCONUT_REQUIRE_EQ(data_val, std::string("hello"));
}

COCONUT_TEST(integration, bridge_toJson_lua_table_to_json) {
  sol::state lua;
  lua.open_libraries();

  sol::table t = lua.create_table();
  t["name"] = "Bob";
  t["score"] = 42;

  nlohmann::json j = coconut::bridge::toJson(t);
  COCONUT_REQUIRE_EQ(j["name"], "Bob");
  COCONUT_REQUIRE_EQ(j["score"], 42);
}

COCONUT_TEST(integration, bridge_toJson_lua_array_to_json) {
  sol::state lua;
  lua.open_libraries();

  sol::table t = lua.create_table();
  t[1] = "x";
  t[2] = "y";
  t[3] = "z";

  nlohmann::json j = coconut::bridge::toJson(t);
  COCONUT_REQUIRE(j.is_array());
  COCONUT_REQUIRE_EQ(j.size(), size_t(3));
  COCONUT_REQUIRE_EQ(j[0], "x");
  COCONUT_REQUIRE_EQ(j[2], "z");
}

COCONUT_TEST(integration, bridge_roundtrip_json_lua_json) {
  sol::state lua;
  lua.open_libraries();

  nlohmann::json original = {{"msg", "hello"}, {"count", 7}, {"tags", {"a", "b"}}};

  sol::table t = coconut::bridge::toTable(lua, original);
  nlohmann::json result = coconut::bridge::toJson(t);

  COCONUT_REQUIRE_EQ(result["msg"].get<std::string>(), original["msg"].get<std::string>());
  COCONUT_REQUIRE_EQ(result["count"].get<int>(), original["count"].get<int>());
  COCONUT_REQUIRE(result["tags"].is_array());
  COCONUT_REQUIRE_EQ(result["tags"].size(), original["tags"].size());
  COCONUT_REQUIRE_EQ(result["tags"][0].get<std::string>(), original["tags"][0].get<std::string>());
}
