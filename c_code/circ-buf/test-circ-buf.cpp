#include <gtest/gtest.h>

// C++ libraries
#include <iostream>
#include <limits>

// Library to test
#include "circ-buf.hpp"

#define TARGET_SIZE (10U)

TEST(CircularBuffer, PushBack) {
  CircularBuffer<uint8_t, TARGET_SIZE> circBuf;

  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
  ASSERT_EQ(0, circBuf.size());

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Push back one more... should fail
  ASSERT_TRUE(circBuf.push_back(100) == false);
  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
  ASSERT_EQ(TARGET_SIZE, circBuf.size());
}

TEST(CircularBuffer, ForwardIterator) {
  CircularBuffer<uint8_t, TARGET_SIZE> circBuf;

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Forward Iterators: Test postfix ++ operator
  uint8_t val = 1;
  for (auto it = circBuf.begin(); it != circBuf.end();) {
    ASSERT_EQ(val++, *(it++));
  }
  ASSERT_EQ(val, TARGET_SIZE + 1);

  // Forward Iterators: Test prefix ++ operator
  val = 1;
  for (auto it = circBuf.begin(); val < TARGET_SIZE;) {
    ASSERT_EQ(++val, *(++it));
  }
  ASSERT_EQ(val, TARGET_SIZE);

  // Forward Iterators: Test postfix -- operator
  val = circBuf.size();
  for (auto it = circBuf.end(); it != circBuf.begin();) {
    // Decrement 'it' the first time only
    if (it == circBuf.end()) {
      it--;
    }

    ASSERT_EQ(val--, *(it--));
  }
  ASSERT_EQ(val, 1);

  // Forward Iterators: Test prefix -- operator
  val = circBuf.size() + 1;
  for (auto it = circBuf.end(); it != circBuf.begin();) {
    ASSERT_EQ(--val, *(--it));
  }
  ASSERT_EQ(val, 1);

  // Check basic logic: decrementing "end" is the same as back()
  auto end = circBuf.end();
  ASSERT_EQ(circBuf.back(), *(--end));
}

TEST(CircularBuffer, ReverseIterator) {
  CircularBuffer<uint8_t, TARGET_SIZE> circBuf;

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Reverse Iterators: Test postfix ++ operator
  uint8_t val = circBuf.size();
  for (auto it = circBuf.rbegin(); it != circBuf.rend();) {
    ASSERT_EQ(val--, *(it++));
  }
  ASSERT_EQ(val, 0);

  // Reverse Iterators: Test prefix ++ operator
  val = circBuf.size();
  for (auto it = circBuf.rbegin(); it != circBuf.rend();) {
    ASSERT_EQ(--val, *(++it));
  }
  ASSERT_EQ(val, 0);

  // Reverse Iterators: Test postfix -- operator
  val = 1;
  for (auto it = circBuf.rend(); it != circBuf.rbegin();) {
    // Decrement 'it' the first time only
    if (it == circBuf.rend()) {
      it--;
    }

    ASSERT_EQ(val++, *(it--));
  }
  ASSERT_EQ(val, TARGET_SIZE);

  // Reverse Iterators: Test prefix -- operator
  val = 0;
  for (auto it = circBuf.rend(); it != circBuf.rbegin();) {
    ASSERT_EQ(++val, *(--it));
  }
  ASSERT_EQ(val, TARGET_SIZE);

  // Check basic logic: decrementing "rend" is the same as front()
  auto rend = circBuf.rend();
  ASSERT_EQ(circBuf.front(), *(--rend));
}

TEST(CircularBuffer, Accessors) {
  CircularBuffer<uint8_t, TARGET_SIZE> circBuf;

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Verify front() and back()
  ASSERT_EQ(circBuf.front(), 1);
  ASSERT_EQ(circBuf.back(), TARGET_SIZE);

  // Verify data via at()
  for (uint8_t i = 0; i < TARGET_SIZE; i++) {
    ASSERT_EQ(circBuf.at(i), i + 1);
  }

  // Verify data via [] operator
  for (uint8_t i = 0; i < TARGET_SIZE; i++) {
    ASSERT_EQ(circBuf[i], i + 1);
  }
}

TEST(CircularBuffer, Rotate) {
  CircularBuffer<uint8_t, TARGET_SIZE> circBuf;

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Push back one more... should fail
  ASSERT_TRUE(circBuf.push_back(100) == false);
  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
  ASSERT_EQ(TARGET_SIZE, circBuf.size());

  // Consume a few items
  uint8_t nRemove = TARGET_SIZE / 2;
  for (uint8_t i = 1; i <= nRemove; i++) {
    circBuf.pop_front();
    ASSERT_EQ(i + 1, circBuf.front());
    ASSERT_EQ(TARGET_SIZE - i, circBuf.size());
  }

  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());

  // Insert same number of items back
  uint8_t nAdd = nRemove;
  for (uint8_t i = 0; i < nAdd; i++) {
    circBuf.push_back(TARGET_SIZE + i);
    ASSERT_EQ(TARGET_SIZE + i, circBuf.back());
    ASSERT_EQ(TARGET_SIZE - nRemove + i + 1, circBuf.size());
  }

  ASSERT_EQ(nRemove + 1, circBuf.front());

  // Push back one more... should fail again
  ASSERT_TRUE(circBuf.push_back(100) == false);

  // Clean-up: Remove all items
  for (uint8_t i = 1; i <= TARGET_SIZE; i++) {
    circBuf.pop_front();
    ASSERT_EQ(TARGET_SIZE - i, circBuf.size());
  }

  // Call pop_front() one more time, should do nothing
  circBuf.pop_front();
  ASSERT_EQ(0, circBuf.size());
  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
}

TEST(CircularBuffer, MixedFunctionality) {
  // Create a circular buffer with capacity 3
  CircularBuffer<uint8_t, 3> buf;

  // Test empty and size
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.size(), 0);

  // Test push_back and size
  EXPECT_TRUE(buf.push_back(1));
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), 1);

  // Test front, back, at, and operator[]
  EXPECT_EQ(buf.front(), 1);
  EXPECT_EQ(buf.back(), 1);
  EXPECT_EQ(buf.at(0), 1);
  EXPECT_EQ(buf[0], 1);

  // Test push_back and size again
  EXPECT_TRUE(buf.push_back(2));
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), 2);

  // Test front, back, at, and operator[]
  EXPECT_EQ(buf.front(), 1);
  EXPECT_EQ(buf.back(), 2);
  EXPECT_EQ(buf.at(0), 1);
  EXPECT_EQ(buf.at(1), 2);
  EXPECT_EQ(buf[0], 1);
  EXPECT_EQ(buf[1], 2);

  // Test pop_front and size
  buf.pop_front();
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), 1);

  // Test front, back, at, and operator[]
  EXPECT_EQ(buf.front(), 2);
  EXPECT_EQ(buf.back(), 2);
  EXPECT_EQ(buf.at(0), 2);
  EXPECT_EQ(buf[0], 2);

  // Test push_back beyond capacity
  EXPECT_TRUE(buf.push_back(3));
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), 2);

  // Test push_back and pop_front to overwrite buffer
  EXPECT_TRUE(buf.push_back(4));  // Size is now 3, further pushes should fail
  EXPECT_FALSE(buf.push_back(5));
  EXPECT_FALSE(buf.push_back(6));
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), 3);
  buf.pop_front();
  EXPECT_EQ(buf.front(), 3);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
