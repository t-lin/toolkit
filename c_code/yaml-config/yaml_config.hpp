#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <type_traits>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace {

// Template overrides to determine if vector, map, or list.
// Specifically check for these types because yaml-cpp supports them.
template <typename T>
struct isVecList : std::false_type {};

template <typename T, typename A>
struct isVecList<std::vector<T, A>> : std::true_type {};

template <typename T, typename A>
struct isVecList<std::list<T, A>> : std::true_type {};

template <typename T>
struct isMap : std::false_type {};

template <typename T, typename... Ts>
struct isMap<std::map<T, Ts...>> : std::true_type {};

// Bug: Parsing of value breaks when yml_t is uint8_t or int8_t, or their
//      vector/list/map container equivalents.
//      See: https://github.com/jbeder/yaml-cpp/issues/1081
//
//      For now, detect these cases and throw an error.
template <typename yml_t>
static void checkBadTypes() {
  static_assert(!std::is_same<yml_t, uint8_t>(),
                "yaml-cpp does not support uint8_t");
  static_assert(!std::is_same<yml_t, int8_t>(),
                "yaml-cpp does not support int8_t");
  if constexpr (isVecList<yml_t>::value) {
    static_assert(!std::is_same<typename yml_t::value_type, uint8_t>(),
                  "yaml-cpp does not support vector/list of uint8_t");
    static_assert(!std::is_same<typename yml_t::value_type, int8_t>(),
                  "yaml-cpp does not support vector/list of int8_t");
  } else if constexpr (isMap<yml_t>::value) {
    static_assert(!std::is_same<typename yml_t::mapped_type, uint8_t>(),
                  "yaml-cpp does not support map of uint8_t");
    static_assert(!std::is_same<typename yml_t::mapped_type, int8_t>(),
                  "yaml-cpp does not support map of int8_t");
  }
}

}  // unnamed namespace

namespace yaml_config {

// this can be defined as an environment variable, which override the config
// found in cmake, which points to the config in this package's installation

static YAML::Node __get_yaml_config() {
  char* __config_env = std::getenv("YAML_CONFIG_PATH");
  if (__config_env != nullptr) {
    try {
      return YAML::LoadFile(__config_env);
    } catch (const std::exception& e) {
      std::cerr << "Override YAML_CONFIG_PATH env var failed. Falling "
                   "back to default path.\n";
    }
  }

  return YAML::LoadFile(YAML_CONFIG_PATH);
}

inline YAML::Node config = __get_yaml_config();

// below are helper functions, but config could just be used directly.

template <typename yml_t = std::string>
std::optional<yml_t> getWithOptional(
    std::string package,
    std::string key,
    std::optional<YAML::NodeType::value> required_type = {},
    YAML::Node node = config) {
  // TODO: Remove this check when bug fixed
  checkBadTypes<yml_t>();

  try {
    if (YAML::Node val = node[package][key]) {
      if (required_type && val.Type() != *required_type) {
        return {};
      }
      return val.as<yml_t>();
    } else {
      return {};
    }
  } catch (const std::exception& e) {
    return {};
  }
}

template <typename yml_t = std::string>
yml_t getWithDefault(std::string package,
                     std::string key,
                     yml_t def_val,
                     YAML::Node node = config) {
  if (auto val = getWithOptional<yml_t>(package, key, {}, node)) {
    return *val;
  } else {
    return def_val;
  }
}

template <typename yml_t = std::string>
yml_t get(std::string package, std::string key, YAML::Node node = config) {
  return getWithDefault<yml_t>(package, key, yml_t(), node);
}

template <typename yml_t = std::string>
std::optional<yml_t> getWithOptional(
    std::string key,
    std::optional<YAML::NodeType::value> required_type = {},
    YAML::Node node = config) {
  checkBadTypes<yml_t>();
  try {
    if (YAML::Node val = node[key]) {
      if (required_type && val.Type() != *required_type) {
        return {};
      }
      return val.as<yml_t>();
    } else {
      return {};
    }
  } catch (const std::exception& e) {
    return {};
  }
}

template <typename yml_t = std::string>
yml_t getWithDefault(std::string key, yml_t def_val, YAML::Node node = config) {
  if (auto val = getWithOptional<yml_t>(key, {}, node)) {
    return *val;
  } else {
    return def_val;
  }
}

template <typename yml_t = std::string>
yml_t get(std::string key, YAML::Node node = config) {
  return getWithDefault<yml_t>(key, yml_t(), node);
}

}  // namespace yaml_config
