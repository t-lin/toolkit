#include <assert.h>

#include <iostream>
#include <type_traits>

using namespace std;

// Obtains the underlying type of an enum or other aliased type.
template <typename E>
using base_t = std::underlying_type_t<E>;

// Converts a variable belonging to an enum or other aliased type to
// its underlying type.
template <typename E, typename BT = std::underlying_type_t<E>>
constexpr BT toBaseType(E e) {
  return static_cast<BT>(e);
}

/**
 * @brief Given a string, attempts to parse out the last set of characters
 *        that are valid for variables.
 *        e.g. Given: "Base::MemberVar", returns "MemberVar".
 *
 * @param name A valid C-string (i.e. NULL-terminated sequence of chars)
 *
 * @return A C++ string containing the parsed string.
 */
string ParseLastVariable(const char* name) {
  uint16_t strSize = __builtin_strlen(name);
  int16_t idx = 0, endIdx = -1, beginIdx = -1;

  for (idx = strSize; idx >= 0; idx--) {
    if (endIdx == -1 && isalpha(name[idx])) {
      endIdx = idx;
    }

    if (beginIdx == -1 && endIdx != -1 &&
        (isalpha(name[idx]) || name[idx] == '_') == false) {
      beginIdx = idx + 1;
      break;
    } else if (idx == 0) {
      beginIdx = idx;
    }
  }

  return string(name + beginIdx, endIdx - beginIdx + 1);
}

/**
 * @brief Get the string name of an enum value, with the enum's value as
 *        a template argument. Only works with Clang or gcc.
 *
 * @return The name of the enum value, as a string.
 */
template <auto V>
constexpr auto EnumStr() noexcept {
  // NOTE: The return type *must* be left as 'auto' for this to work.
  // This function is partially inspired by Liem Dam-Quang's Janus enum_utils.
  static_assert(std::is_enum_v<decltype(V)>, "ERROR: Not an enum");

#if defined(__clang__) || defined(__GNUC__)
  return ParseLastVariable(__PRETTY_FUNCTION__);
#else
#error "Unsupported compiler. Must use Clang 5.0+ or gcc 9.0+."
#endif
}

enum TestEnum : uint8_t {
  TEN = 10,
  TWENTY = 20,
  THIRTY = 30,
};

int main() {
  // Printing names of enum literals
  printf("TestEnum::TEN is: %s\n", EnumStr<TestEnum::TEN>().c_str());
  printf("TestEnum::TWENTY is: %s\n", EnumStr<TestEnum::TWENTY>().c_str());
  printf("TestEnum::THIRTY is: %s\n", EnumStr<TestEnum::THIRTY>().c_str());

  // Type conversions
  static_assert(std::is_same_v<base_t<TestEnum>, uint8_t>);

  TestEnum testEnum = TestEnum::TWENTY;
  auto baseTypeVal = toBaseType(testEnum);
  assert(baseTypeVal == 20);

  return 0;
}
