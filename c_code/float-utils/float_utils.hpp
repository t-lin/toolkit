#pragma once

//C library headers
#include <stdint.h>
#include <math.h>
#include <stdio.h> // For debugging

// C++ library headers
#include <type_traits>

namespace FloatUtils {

namespace _internal {

template <uint8_t decimalPlaces>
constexpr uint32_t powerOfTen() {
  static_assert(decimalPlaces <= 10, "No more than 10 decimal places allowed");

  // Faster than pow()
  uint8_t decimals = decimalPlaces; // Copy for editing
  uint32_t res = 1;
  while( decimals > 0 ) {
    res *= 10;
    decimals--;
  }

  return res;
}

}

// Round floating-point values to 'decimalPlaces' worth of decimals,
// then compares their equality.
//  - e.g. 1.114 with 2 decimal places => 1.11
//         1.115 with 2 dicimal places => 1.12
template <typename T, uint8_t decimalPlaces>
inline bool floatEqualTo( const T lhs, const T rhs ) {
  static_assert(std::is_floating_point_v<T> == true,
      "Template type must be a floating point type" );

  constexpr uint32_t multiplier = _internal::powerOfTen<decimalPlaces>();

  // Always use doubles for increased precision.
  const double leftVal = lhs, rightVal = rhs;

  return std::llround( leftVal * multiplier ) == std::llround( rightVal * multiplier );
}

template <typename T, uint8_t decimalPlaces>
inline bool floatGreaterEqualThan( const T lhs, const T rhs ) {
  static_assert(std::is_floating_point_v<T> == true,
      "Template type must be a floating point type" );

  constexpr uint32_t multiplier = _internal::powerOfTen<decimalPlaces>();

  // Always use doubles for increased precision.
  const double leftVal = lhs, rightVal = rhs;

  return std::llround( leftVal * multiplier ) >= std::llround( rightVal * multiplier );
}

template <typename T, uint8_t decimalPlaces>
inline bool floatLessEqualThan( const T lhs, const T rhs ) {
  static_assert(std::is_floating_point_v<T> == true,
      "Template type must be a floating point type" );

  constexpr uint32_t multiplier = _internal::powerOfTen<decimalPlaces>();

  // Always use doubles for increased precision.
  const double leftVal = lhs, rightVal = rhs;

  return std::llround( leftVal * multiplier ) <= std::llround( rightVal * multiplier );
}

template <typename T, uint8_t decimalPlaces>
inline bool floatNotEqualTo( const T lhs, const T rhs ) {
  return floatEqualTo<T, decimalPlaces>(lhs, rhs) == false;
}

template <typename T, uint8_t decimalPlaces>
inline bool floatGreaterThan( const T lhs, const T rhs ) {
  return floatLessEqualThan<T, decimalPlaces>(lhs, rhs) == false;
}

template <typename T, uint8_t decimalPlaces>
inline bool floatLessThan( const T lhs, const T rhs ) {
  return floatGreaterEqualThan<T, decimalPlaces>(lhs, rhs) == false;
}

} // FloatUtils namespace
