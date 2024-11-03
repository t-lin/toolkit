#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

// C++ libraries
#include <exception>

#include "float_utils.hpp"
#include "../gtest-extras/test_utils.hpp"

using namespace FloatUtils;

using std::exception;

/*******************
 * Tests
 *******************/
TEST(FloatUtilsTest, EqualTo) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_TRUE( (floatEqualTo<float, 5>(a, b)) );

  // Compare to 6 decimal places
  EXPECT_FALSE( (floatEqualTo<float, 6>(a, b)) );

  // Compare to 7 decimal places
  EXPECT_FALSE( (floatEqualTo<float, 7>(a, b)) );
}

TEST(FloatUtilsTest, NotEqualTo) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_FALSE( (floatNotEqualTo<float, 5>(a, b)) );

  // Compare to 6 decimal places
  EXPECT_TRUE( (floatNotEqualTo<float, 6>(a, b)) );

  // Compare to 7 decimal places
  EXPECT_TRUE( (floatNotEqualTo<float, 7>(a, b)) );
}

TEST(FloatUtilsTest, GreaterEqualThan) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_TRUE( (floatGreaterEqualThan<float, 5>(a, b)) );
  EXPECT_TRUE( (floatGreaterEqualThan<float, 5>(b, a)) );

  // Compare to 6 decimal places
  EXPECT_FALSE( (floatGreaterEqualThan<float, 6>(a, b)) );
  EXPECT_TRUE( (floatGreaterEqualThan<float, 6>(b, a)) );

  // Compare to 7 decimal places
  EXPECT_FALSE( (floatGreaterEqualThan<float, 7>(a, b)) );
  EXPECT_TRUE( (floatGreaterEqualThan<float, 7>(b, a)) );
}

TEST(FloatUtilsTest, GreaterThan) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_FALSE( (floatGreaterThan<float, 5>(a, b)) );
  EXPECT_FALSE( (floatGreaterThan<float, 5>(b, a)) );

  // Compare to 6 decimal places
  EXPECT_FALSE( (floatGreaterThan<float, 6>(a, b)) );
  EXPECT_TRUE( (floatGreaterThan<float, 6>(b, a)) );

  // Compare to 7 decimal places
  EXPECT_FALSE( (floatGreaterThan<float, 7>(a, b)) );
  EXPECT_TRUE( (floatGreaterThan<float, 7>(b, a)) );
}

TEST(FloatUtilsTest, LessEqualThan) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_TRUE( (floatLessEqualThan<float, 5>(a, b)) );
  EXPECT_TRUE( (floatLessEqualThan<float, 5>(b, a)) );

  // Compare to 6 decimal places
  EXPECT_TRUE( (floatLessEqualThan<float, 6>(a, b)) );
  EXPECT_FALSE( (floatLessEqualThan<float, 6>(b, a)) );

  // Compare to 7 decimal places
  EXPECT_TRUE( (floatLessEqualThan<float, 7>(a, b)) );
  EXPECT_FALSE( (floatLessEqualThan<float, 7>(b, a)) );
}

TEST(FloatUtilsTest, LessThan) {
  float a = 0.123456f, b = 0.1234567f;

  // Ugh, EXPECT_TRUE() doesn't play well w/ templates... so we need to
  // wrap the statements w/ extra parentheses.

  // Compare to 5 decimal places
  EXPECT_FALSE( (floatLessThan<float, 5>(a, b)) );
  EXPECT_FALSE( (floatLessThan<float, 5>(b, a)) );

  // Compare to 6 decimal places
  EXPECT_TRUE( (floatLessThan<float, 6>(a, b)) );
  EXPECT_FALSE( (floatLessThan<float, 6>(b, a)) );

  // Compare to 7 decimal places
  EXPECT_TRUE( (floatLessThan<float, 7>(a, b)) );
  EXPECT_FALSE( (floatLessThan<float, 7>(b, a)) );
}

int main(int argc, char** argv) {
  int ret = 0;
  try {
    ::testing::InitGoogleTest(&argc, argv);
     ret = RUN_ALL_TESTS();
  } catch (exception& exc) {
    std::cerr << exc.what() << std::endl;
  }

  return ret;
}
