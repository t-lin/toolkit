#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

// Set 'YAML_CONFIG_PATH' environment variable to test file.
// This is usually set by the user / callee of the program executable.
// Needs to be done before main() and any static/globals are instantiated.
void __attribute__((constructor)) SetConfigPath() {
  setenv("YAML_CONFIG_PATH", YAML_TEST_CONFIG_PATH, 1);
}

// C++ libs
#include <map>
#include <string>
#include <vector>

// C libs
#include <stdint.h>
#include <stdlib.h>

#include "yaml_config.hpp"

using yaml_config::get;
using yaml_config::getWithDefault;
using yaml_config::getWithOptional;

using std::map;
using std::string;
using std::vector;

// Fetch globals
TEST(ConfigTest, TestGlobalConfig) {
  ASSERT_TRUE(get<bool>("verbose"));
  ASSERT_TRUE(getWithDefault<bool>("verbose", false));

  ASSERT_FALSE(get<bool>("debug"));
  ASSERT_FALSE(getWithDefault<bool>("debug", true));

  ASSERT_EQ(get<int32_t>("priority"), 1000);
  ASSERT_EQ(getWithDefault<int32_t>("priority", -10), 1000);

  ASSERT_EQ(get<string>("path"), "some_string");
  ASSERT_EQ(getWithDefault<string>("path", "asdf"), "some_string");
}

// Fetch values from section w/ same name as globals
TEST(ConfigTest, TestOverride) {
  const char* SECTION_NAME = "override_test";

  ASSERT_FALSE(get<bool>(SECTION_NAME, "verbose"));
  ASSERT_FALSE(getWithDefault<bool>(SECTION_NAME, "verbose", true));

  ASSERT_TRUE(get<bool>(SECTION_NAME, "debug"));
  ASSERT_TRUE(getWithDefault<bool>(SECTION_NAME, "debug", false));

  ASSERT_EQ(get<int32_t>(SECTION_NAME, "priority"), 2000);
  ASSERT_EQ(getWithDefault<int32_t>(SECTION_NAME, "priority", -10), 2000);

  ASSERT_EQ(get<string>(SECTION_NAME, "path"), "some_other_string");
  ASSERT_EQ(getWithDefault<string>(SECTION_NAME, "path", "asdf"),
            "some_other_string");
}

TEST(ConfigTest, TestFirstLvlList) {
  const char* SECTION_NAME = "first_level_list";

  const vector<string> EXPECTED_STRS = {"string1", "string2"};

  auto listStrs = get<vector<string>>(SECTION_NAME);
  ASSERT_EQ(EXPECTED_STRS.size(), listStrs.size());
  for (uint8_t i = 0; i < EXPECTED_STRS.size(); i++) {
    ASSERT_EQ(EXPECTED_STRS[i], listStrs[i]);
  }
}

TEST(ConfigTest, TestLists) {
  const char* SECTION_NAME = "list_test";

  // List of strings
  const vector<string> EXPECTED_STRS = {"string1", "string2", "string3"};
  auto listStrs = get<vector<string>>(SECTION_NAME, "list_strings");

  ASSERT_EQ(EXPECTED_STRS.size(), listStrs.size());
  for (uint8_t i = 0; i < EXPECTED_STRS.size(); i++) {
    ASSERT_EQ(EXPECTED_STRS[i], listStrs[i]);
  }

  // List of ints
  // NOTE: Bug w/ parsing u/int8_ts, currently must use 16-bit or higher
  const vector<int16_t> EXPECTED_INTS = {-1, -2, 3};
  auto listInts = get<vector<int16_t>>(SECTION_NAME, "list_ints");

  ASSERT_EQ(EXPECTED_INTS.size(), listInts.size());
  for (uint8_t i = 0; i < EXPECTED_STRS.size(); i++) {
    ASSERT_EQ(EXPECTED_INTS[i], listInts[i]);
  }

  // Try parsing as list of uints
  // NOTE: Bug w/ parsing u/int8_ts, currently must use 16-bit or higher
  const vector<uint16_t> EXPECTED_UINTS = {1, 2, 3};
  auto listUints = get<vector<uint16_t>>(SECTION_NAME, "list_uints");

  ASSERT_EQ(EXPECTED_UINTS.size(), listUints.size());
  for (uint8_t i = 0; i < EXPECTED_UINTS.size(); i++) {
    ASSERT_EQ(EXPECTED_UINTS[i], listUints[i]);
  }
}

TEST(ConfigTest, IntBoundTests) {
  const char* SECTION_NAME = "int_bounds";

  ASSERT_EQ(getWithDefault<uint16_t>(SECTION_NAME, "uint16_max", 123), 65535);
  ASSERT_EQ(getWithDefault<int16_t>(SECTION_NAME, "int16_min", 123), -32768);

  ASSERT_EQ(getWithDefault<uint16_t>(SECTION_NAME, "gt_uint16", 123), 123);
  ASSERT_EQ(getWithDefault<int16_t>(SECTION_NAME, "lt_int16", 123), 123);
}

TEST(ConfigTest, MapTest) {
  const char* SECTION_NAME = "map_test";

  map<string, uint16_t> EXPECTED_PAIRS = {
      {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}
  };

  auto readMap = get<map<string, uint16_t>>(SECTION_NAME);
  for (auto& kv : EXPECTED_PAIRS) {
    ASSERT_EQ(readMap[kv.first], kv.second);
  }
}

TEST(ConfigTest, TestgetWithOptional) {
  YAML::Node node;
  node["tester1"] = (float)5.1;
  node["tester2"] = (int)5;
  node["map_tester"]["a"] = 1;
  node["map_tester"]["b"] = 2;
  node["map_tester"]["c"] = 3;

  // no key
  if (auto val = getWithOptional<int>("nokey", {}, node)) {
    FAIL();
  }

  // wrong type
  if (auto val = getWithOptional<int>("tester1", {}, node)) {
    FAIL();
  }

  // success
  if (auto val = getWithOptional<int>("tester2", {}, node)) {
    ASSERT_EQ(*val, 5);
  } else {
    FAIL();
  }

  if (auto val = getWithOptional<YAML::Node>("map_tester", YAML::NodeType::Map,
                                             node)) {
    auto result = *val;
    ASSERT_EQ(result["a"].as<int>(), 1);
    ASSERT_EQ(result["b"].as<int>(), 2);
    ASSERT_EQ(result["c"].as<int>(), 3);
  } else {
    FAIL();
  }

  if (auto val = getWithOptional<YAML::Node>("map_tester",
                                             YAML::NodeType::Sequence, node)) {
    FAIL();
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
