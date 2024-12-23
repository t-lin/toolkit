#include <gtest/gtest.h>

// C++ libraries
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

// Library to test
#include "bounded-fifo.hpp"

#define TARGET_SIZE (10U)

// Define complex type for testing
struct TestObject {
  char a = 'A';
  double b = 3.14;
  uint16_t c = 42;
};

/************************
 * Generic test helpers
 ************************/

template <uint64_t CAP>
BoundedFIFO<uint8_t, CAP> createBufferNoRotatation(uint8_t sz = CAP) {
  // Should not add more than the capacity.
  if (sz > CAP) {
    throw std::invalid_argument("sz cannot be greater than CAP\n");
  }

  BoundedFIFO<uint8_t, CAP> circBuf;

  // Add some data; 1-indexed
  for (uint8_t val = 1; val <= sz; val++) {
    circBuf.push_back(val);
  }

  return circBuf;
}

template <uint64_t CAP>
BoundedFIFO<uint8_t, CAP> createRotatedBuffer(uint8_t nRotate) {
  // Should not rotate same amount or greater than TARGET_SIZE.
  // Okay, in theory you can rotate more than TARGET_SIZE but let's be strict.
  if (nRotate >= CAP) {
    throw std::invalid_argument("nRotate should be less than CAP\n");
  }

  BoundedFIFO<uint8_t, CAP> circBuf =
      createBufferNoRotatation<CAP>(TARGET_SIZE);

  // Pop some data, then add back.
  const uint8_t highestNum = circBuf.back();
  for (uint8_t val = 1; val <= nRotate; val++) {
    circBuf.pop_front();
    circBuf.push_back(highestNum + val);
  }

  return circBuf;
}

BoundedFIFO<uint8_t, TARGET_SIZE> createRotatedPartialBuffer(
    const uint8_t nRemove,
    const uint8_t nAdd) {

  // Point of this helper is to have a partially full buffer... so we should
  // always remove more than we add back.
  if (nRemove <= nAdd) {
    throw std::invalid_argument("nRemove must be > than nAdd\n");
  }

  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);

  // Pop some data, then add back less than what we popped.
  for (uint8_t i = 0; i < nRemove; i++) {
    circBuf.pop_front();
  }

  for (uint8_t i = 1; i <= nAdd; i++) {
    circBuf.push_back(TARGET_SIZE + i);
  }

  return circBuf;
}

/************************
 * Iterator test helpers
 ************************/

// NOTE: These tests assume that sequential values are stored in the circular
//       buffer from the head to the tail.
//       Also, these tests assume that front() and back() works correctly.
void forwardIteratorTests(BoundedFIFO<uint8_t, TARGET_SIZE>& circBuf) {
  // Forward Iterators: Test postfix ++ operator
  uint8_t val = circBuf.front();
  for (auto it = circBuf.cbegin(); it != circBuf.cend();) {
    ASSERT_EQ(val++, *(it++));
  }
  ASSERT_EQ(val, circBuf.back() + 1);

  // Forward Iterators: Test prefix ++ operator
  // Loop condition avoids dereferencing end().
  val = circBuf.front();
  for (auto it = circBuf.cbegin(); *it < circBuf.back();) {
    ASSERT_EQ(++val, *(++it));
  }
  ASSERT_EQ(val, circBuf.back());

  // Forward Iterators: Test postfix -- operator
  val = circBuf.back();
  for (auto it = circBuf.cend() - 1; it != circBuf.cbegin();) {
    ASSERT_EQ(val--, *(it--));
  }
  ASSERT_EQ(val, circBuf.front());

  // Forward Iterators: Test prefix -- operator
  val = circBuf.back() + 1;
  for (auto it = circBuf.cend(); it != circBuf.cbegin();) {
    ASSERT_EQ(--val, *(--it));
  }
  ASSERT_EQ(val, circBuf.front());

  // Check basic logic: decrementing "end" is the same as back()
  auto end = circBuf.cend();
  ASSERT_EQ(circBuf.back(), *(--end));

  // Check basic logic: increment "end" doesn't change it
  auto end2 = circBuf.cend();
  ASSERT_EQ(circBuf.cend(), ++end2);
  ASSERT_EQ(circBuf.cend(), ++end2); // Test again, for sanity
}

// NOTE: These tests assume that sequential values are stored in the circular
//       buffer from the head to the tail.
//       Also, these tests assume that back() and front() works correctly.
void reverseIteratorTests(BoundedFIFO<uint8_t, TARGET_SIZE>& circBuf) {
  // Reverse Iterators: Test postfix ++ operator
  uint8_t val = circBuf.back();
  for (auto it = circBuf.crbegin(); it != circBuf.crend();) {
    ASSERT_EQ(val--, *(it++));
  }
  ASSERT_EQ(val, circBuf.front() - 1);

  // Reverse Iterators: Test prefix ++ operator
  // Loop condition avoids dereferencing rend().
  val = circBuf.back();
  for (auto it = circBuf.crbegin(); *it > circBuf.front();) {
    ASSERT_EQ(--val, *(++it));
  }
  ASSERT_EQ(val, circBuf.front());

  // Reverse Iterators: Test postfix -- operator
  val = circBuf.front();
  for (auto it = circBuf.crend() - 1; it != circBuf.crbegin();) {
    ASSERT_EQ(val++, *(it--));
  }
  ASSERT_EQ(val, circBuf.back());

  // Reverse Iterators: Test prefix -- operator
  val = circBuf.front() - 1;
  for (auto it = circBuf.crend(); it != circBuf.crbegin();) {
    ASSERT_EQ(++val, *(--it));
  }
  ASSERT_EQ(val, circBuf.back());

  // Check basic logic: decrementing "rend" is the same as front()
  auto rend = circBuf.crend();
  ASSERT_EQ(circBuf.front(), *(--rend));

  // Check basic logic: increment "rend" doesn't change it
  auto rend2 = circBuf.crend();
  ASSERT_EQ(circBuf.crend(), ++rend2);
  ASSERT_EQ(circBuf.crend(), ++rend2); // Test again, for sanity
}


/*******************************
 * Start gtests; BoundedFIFO
 *******************************/
TEST(BoundedFIFO, PushBack_Size_MaxSize) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf;

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

TEST(BoundedFIFO, PopFront_Front_Back) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf;

  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
  ASSERT_EQ(0, circBuf.size());

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(val, circBuf.size());

    // Verify front() and back()
    ASSERT_EQ(circBuf.front(), 1);
    ASSERT_EQ(circBuf.back(), val);
  }

  // Pop data one by one
  for (uint8_t val = 1; val <= TARGET_SIZE; val++)
  {
    ASSERT_EQ(circBuf.front(), val);
    circBuf.pop_front();

    // Verify front() and back()
    if (val < TARGET_SIZE) {
      ASSERT_EQ(circBuf.front(), val + 1);
      ASSERT_EQ(circBuf.back(), TARGET_SIZE);
    } else {
      // Verify it throws when the buffer is empty
      ASSERT_THROW(circBuf.front(), std::runtime_error);
      ASSERT_THROW(circBuf.back(), std::runtime_error);
    }
  }
}

TEST(BoundedFIFO, Accessors) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);

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

  // Test throwing out-of-range exception when accessing out-of-bounds.
  ASSERT_THROW(circBuf[TARGET_SIZE + 10], std::out_of_range);
}

TEST(BoundedFIFO, Rotate) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf;

  // Add some data
  for (uint8_t val = 1; val <= TARGET_SIZE; val++) {
    ASSERT_TRUE(circBuf.push_back(val) == true);
    ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
    ASSERT_EQ(val, circBuf.size());
  }

  // Underlying array now should be:
  //  [1, 2, 3, ... TARGET_SIZE - 2, TARGET_SIZE - 1, TARGET_SIZE]

  // Push back one more... should fail
  ASSERT_TRUE(circBuf.push_back(100) == false);
  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());
  ASSERT_EQ(TARGET_SIZE, circBuf.size());

  // Consume half of the buffer's items
  const uint8_t nRemove = TARGET_SIZE / 2;
  for (uint8_t i = 1; i <= nRemove; i++) {
    circBuf.pop_front();
    ASSERT_EQ(i + 1, circBuf.front());
    ASSERT_EQ(TARGET_SIZE - i, circBuf.size());
  }

  // Underlying array now should be (x := element not "in buffer"):
  //  [x, x, ... TARGET_SIZE/2, TARGET_SIZE/2 + 1, ... TARGET_SIZE - 1, TARGET_SIZE]

  ASSERT_EQ(TARGET_SIZE, circBuf.max_size());

  // Insert same number of items back, incrementing from TARGET_SIZE
  const uint8_t nAdd = nRemove;
  for (uint8_t i = 1; i <= nAdd; i++) {
    circBuf.push_back(TARGET_SIZE + i);
    ASSERT_EQ(TARGET_SIZE + i, circBuf.back());
    ASSERT_EQ(TARGET_SIZE - nRemove + i, circBuf.size());
  }

  // Underlying array now should be:
  //  [TARGET_SIZE + 1, TARGET_SIZE + 2, ... TARGET_SIZE - 2, TARGET_SIZE - 1, TARGET_SIZE]
  //
  // e.g. With a target size of 10:
  //  [11, 12, 13, 14, 15, 6, 7, 8, 9, 10]

  ASSERT_EQ(nRemove + 1, circBuf.front());

  // Sanity check the underlying array.
  const std::array<uint8_t, TARGET_SIZE>* pArray =
    (std::array<uint8_t, TARGET_SIZE>*)(&circBuf);
  for (uint8_t i = 1; i <= TARGET_SIZE; i++) {
    uint8_t expectedVal = (i <= nRemove) ? (TARGET_SIZE + i) : i;
    ASSERT_EQ(expectedVal, pArray->data()[i - 1]); // -1 because 1-indexed
  }

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

TEST(BoundedFIFO, MixedFunctionality) {
  // Create a circular buffer with capacity 3
  BoundedFIFO<uint8_t, 3> buf;

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

TEST(BoundedFIFO, EqualityInequalityOperator) {
  BoundedFIFO<uint8_t, 10> buf1;
  BoundedFIFO<uint8_t, 10> buf2;
  ASSERT_EQ(buf1, buf2);

  buf1.push_back(10);
  buf2.push_back(10);
  ASSERT_EQ(buf1, buf2);

  buf1.pop_front();
  ASSERT_NE(buf1, buf2);
  buf2.pop_front();
  ASSERT_EQ(buf1, buf2);

  buf1.push_back(10);
  buf2.push_back(20);
  ASSERT_NE(buf1, buf2);

  buf1.push_back(100);
  buf1.push_back(200);
  buf2.push_back(100);
  buf2.push_back(200);
  ASSERT_NE(buf1, buf2);

  buf1.pop_front();
  buf2.pop_front();
  ASSERT_EQ(buf1, buf2);
}

TEST(BoundedFIFO, Assignment) {
  BoundedFIFO<uint8_t, TARGET_SIZE> buf1 =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  BoundedFIFO<uint8_t, TARGET_SIZE> buf2 =
      createRotatedBuffer<TARGET_SIZE>(5);

  // Sanity test
  ASSERT_EQ(buf1[0], 1);
  ASSERT_EQ(buf2[0], 6);

  // Assignment test
  buf1 = buf2;
  buf2 = createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  ASSERT_EQ(buf1[0], 6);
}

/**************************
 * BoundedFIFOIterator
 **************************/
TEST(BoundedFIFOIterator, Constructor) {
  BoundedFIFO<uint8_t, TARGET_SIZE> buf;

  // Use lambda to encapsulate the object creation so we can use them as
  // statements for EXPECT_THROW().

  // Test case of providing an initial index >= array size.
  auto fnCreateIteratorThrow1 = [&]() {
    BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it(TARGET_SIZE, &buf);
  };

  ASSERT_THROW(fnCreateIteratorThrow1(), std::invalid_argument);

  // Test case of providing a NULL circular buffer.
  auto fnCreateIteratorThrow2 = [&]() {
    BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it(0, nullptr);
  };

  ASSERT_THROW(fnCreateIteratorThrow2(), std::invalid_argument);
}

TEST(BoundedFIFOIterator, BeginEnd_RBeginREnd_EmptyBuf) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf;

  // Verify empty circBuffer's begin() is the same as end()
  ASSERT_EQ(circBuf.begin(), circBuf.end());

  // Verify empty circBuffer's rbegin() is the same as rend()
  ASSERT_EQ(circBuf.rbegin(), circBuf.rend());

  // Check basic logic: increment "end" doesn't change it
  auto end = circBuf.end();
  ASSERT_EQ(circBuf.end(), ++end);
  ASSERT_EQ(circBuf.end(), ++end); // Test again, for sanity

  // Check basic logic: increment "rend" doesn't change it
  auto rend = circBuf.rend();
  ASSERT_EQ(circBuf.rend(), ++rend);
  ASSERT_EQ(circBuf.rend(), ++rend); // Test again, for sanity
}

TEST(BoundedFIFOIterator, IteratorDerefOp) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf;

  // Verify throws when dereferencing end()
  // TODO: Add verification for throwing on rend() when that's implemented.
  ASSERT_THROW(*(circBuf.end()), std::runtime_error);

  // Populate buffer.
  circBuf = createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);

  // Test dereferencing forward iterators.
  for (uint8_t val = 1; val <= TARGET_SIZE; val++)
  {
    // Explicitly use its constructor instead of starting from begin()/end().
    BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it(val - 1, &circBuf);

    ASSERT_EQ(*it, val);
  }

  // Test dereferencing reverse iterators.
  for (uint8_t val = TARGET_SIZE; val > 0; val--)
  {
    // Explicitly use its constructor instead of starting from begin()/end().
    // Bit tricky, can't use 'val' in the first loop as that's >= N.
    // Thus, we initialize 'it' to the highest index possible, then
    // increment it before passing to the reverse iterator.
    BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it(val - 1, &circBuf);
    BoundedFIFO<uint8_t, TARGET_SIZE>::reverse_iterator revIt(++it);

    ASSERT_EQ(*revIt, val);
  }
}

TEST(BoundedFIFOIterator, IteratorArrowOp) {
  BoundedFIFO<TestObject, TARGET_SIZE> circBuf;

  // Verify throws when using arrow op on end()
  // The (void) is to get rid of "unused-value" warning
  // TODO: Add verification for throwing on rend() when that's implemented.
  ASSERT_THROW((void)circBuf.end()->a, std::runtime_error);

  // Add some data
  TestObject val;
  for (uint8_t i = 0; i < TARGET_SIZE; i++) {
    val.a += static_cast<char>(1);
    val.b += static_cast<double>(1);
    val.c += static_cast<uint16_t>(1);

    ASSERT_TRUE(circBuf.push_back(val) == true);
  }

  // Test dereferencing forward iterators.
  val = TestObject(); // Re-initialize it
  for (uint8_t i = 0; i < TARGET_SIZE; i++) {
    // Explicitly use its constructor instead of starting from begin()/end().
    BoundedFIFO<TestObject, TARGET_SIZE>::iterator it(i, &circBuf);

    val.a += static_cast<char>(1);
    val.b += static_cast<double>(1);
    val.c += static_cast<uint16_t>(1);

    ASSERT_EQ(it->a, val.a);
    ASSERT_EQ(it->b, val.b);
    ASSERT_EQ(it->c, val.c);
  }

  // Test dereferencing reverse iterators.
  for (uint8_t i = TARGET_SIZE; i > 0; i--)
  {
    // Explicitly use its constructor instead of starting from begin()/end().
    // Bit tricky, can't use 'i' in the first loop as that's >= N.
    // Thus, we initialize 'it' to the highest index possible, then
    // increment it before passing to the reverse iterator.
    BoundedFIFO<TestObject, TARGET_SIZE>::iterator it(i - 1, &circBuf);
    BoundedFIFO<TestObject, TARGET_SIZE>::reverse_iterator revIt(++it);

    ASSERT_EQ(revIt->a, val.a);
    ASSERT_EQ(revIt->b, val.b);
    ASSERT_EQ(revIt->c, val.c);

    val.a -= static_cast<char>(1);
    val.b -= static_cast<double>(1);
    val.c -= static_cast<uint16_t>(1);
  }

}

// NoRotate := head is at the beginning of the underlying array.
TEST(BoundedFIFOIterator, ForwardIteratorNoRotatation) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  forwardIteratorTests(circBuf);
}

// NoRotate := head is at the beginning of the underlying array.
TEST(BoundedFIFOIterator, ReverseIteratorNoRotatation) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  reverseIteratorTests(circBuf);
}

// Rotation := head is not at the beginning of the underlying array.
TEST(BoundedFIFOIterator, ForwardIteratorWithRotation) {
  const uint8_t nRotate = TARGET_SIZE / 2;
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createRotatedBuffer<TARGET_SIZE>(nRotate);

  forwardIteratorTests(circBuf);
}

// Rotation := head is not at the beginning of the underlying array.
TEST(BoundedFIFOIterator, ReverseIteratorWithRotation) {
  const uint8_t nRotate = TARGET_SIZE / 2;
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createRotatedBuffer<TARGET_SIZE>(nRotate);

  reverseIteratorTests(circBuf);
}

// PartialBuffer := Underlying buffer is not completely full.
// For complexity, we'll make head somewhere after the tail in the underlying
// buffer (i.e. it wraps around).
TEST(BoundedFIFOIterator, ForwardIteratorPartialBuffer) {
  // Create a buffer, pop some data, then add back less than what we popped.
  const uint8_t nRemove = TARGET_SIZE * 3 / 4; // Remove roughly 3/4
  const uint8_t nAdd = TARGET_SIZE * 1 / 4; // Add back roughly 1/4

  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
    createRotatedPartialBuffer(nRemove, nAdd);

  // Basic sanity checks.
  ASSERT_EQ(circBuf.size(), TARGET_SIZE - nRemove + nAdd);
  ASSERT_EQ(circBuf.front(), nRemove + 1);
  ASSERT_EQ(circBuf.back(), TARGET_SIZE + nAdd);

  forwardIteratorTests(circBuf);
}

// PartialBuffer := Underlying buffer is not completely full.
// For complexity, we'll make head somewhere after the tail in the underlying
// buffer (i.e. it wraps around).
TEST(BoundedFIFOIterator, ReverseIteratorPartialBuffer) {
  // Create a buffer, pop some data, then add back less than what we popped.
  const uint8_t nRemove = TARGET_SIZE * 3 / 4; // Remove roughly 3/4
  const uint8_t nAdd = TARGET_SIZE * 1 / 4; // Add back roughly 1/4

  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
    createRotatedPartialBuffer(nRemove, nAdd);

  // Basic sanity checks.
  ASSERT_EQ(circBuf.size(), TARGET_SIZE - nRemove + nAdd);
  ASSERT_EQ(circBuf.front(), nRemove + 1);
  ASSERT_EQ(circBuf.back(), TARGET_SIZE + nAdd);

  reverseIteratorTests(circBuf);
}

TEST(BoundedFIFOIterator, AssignmentOperator) {
  BoundedFIFO<uint8_t, TARGET_SIZE> buf1 =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  BoundedFIFO<uint8_t, TARGET_SIZE> buf2 =
      createRotatedBuffer<TARGET_SIZE>(5);

  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it1 = buf1.begin();
  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it2 = buf2.begin();

  // Sanity test
  ASSERT_EQ(*it1, 1);
  ASSERT_EQ(*it2, 6);

  // Assignment test
  it1 = it2;
  it2 = buf1.begin();
  ASSERT_EQ(*it1, 6);
  ASSERT_EQ(*it2, 1);
}

TEST(BoundedFIFOIterator, PlusAssignmentOperator) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createRotatedBuffer<TARGET_SIZE>(5);
  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it = circBuf.begin();

  ASSERT_EQ(*it, 6);
  it += (TARGET_SIZE + 1); // Go beyond END, should be END
  ASSERT_EQ(it, circBuf.end());

  it = circBuf.begin();
  it += 1;
  ASSERT_EQ(*it, 7);
  it += 5;
  ASSERT_EQ(*it, 12);
  it += TARGET_SIZE; // Go beyond END; should be END
  ASSERT_EQ(it, circBuf.end());
  it += 1; // Keep adding, should stay at END
  ASSERT_EQ(it, circBuf.end());

  // Test w/ negative numbers
  it += -5;
  ASSERT_EQ(*it, 11);
  it += -5;
  ASSERT_EQ(*it, 6);
  it += -static_cast<int64_t>(TARGET_SIZE); // Go beyond head; should stay at head
  ASSERT_EQ(*it, circBuf.front());
}

TEST(BoundedFIFOIterator, MinusAssignmentOperator) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createRotatedBuffer<TARGET_SIZE>(5);
  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it = circBuf.end();

  ASSERT_EQ(it, circBuf.end());
  it -= 1;
  ASSERT_EQ(*it, 15);
  it -= 5;
  ASSERT_EQ(*it, 10);
  it -= 1; // Test again (curr index > head index)
  ASSERT_EQ(*it, 9);
  it -= TARGET_SIZE; // Go beyond head; should stay at head
  ASSERT_EQ(*it, circBuf.front());

  // Test w/ negative numbers
  it -= -4;
  ASSERT_EQ(*it, 10);
  it -= -3;
  ASSERT_EQ(*it, 13);
  it -= -static_cast<int64_t>(TARGET_SIZE); // Go beyond END; should stay at END
  ASSERT_EQ(it, circBuf.end());
}

TEST(BoundedFIFOIterator, EqualityInequalityOperator) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf2 =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);

  auto it = circBuf.begin();
  auto it2 = circBuf.begin();
  ASSERT_EQ(it, it2);

  // Test comparisons w/ different buffer instances.
  it2 = circBuf2.begin();
  ASSERT_EQ(*it, *it2); // Underlying data should be same
  ASSERT_THROW((void)(it == it2), std::invalid_argument);

  it2++;
  ASSERT_LT(*it, *it2);
  ASSERT_THROW((void)(it < it2), std::invalid_argument);

  it += 2;
  ASSERT_GT(*it, *it2);
  ASSERT_THROW((void)(it > it2), std::invalid_argument);
}

// The following tests are based on semantis defined in:
//  - https://en.cppreference.com/w/cpp/named_req/RandomAccessIterator
TEST(BoundedFIFOIterator, RandomAccessSemantics) {
  BoundedFIFO<uint8_t, TARGET_SIZE> circBuf =
      createBufferNoRotatation<TARGET_SIZE>(TARGET_SIZE);

  // Check addition w/ integers.
  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it1 = circBuf.begin() + 5;
  ASSERT_EQ(*it1, 6);
  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it2 = 5 + circBuf.begin();
  ASSERT_EQ(*it2, 6);

  ASSERT_EQ(it1 + 1, 1 + it2);
  ASSERT_EQ(*(it1 + 1), *(1 + it2));

  // Check that iterators haven't changed from initial values.
  ASSERT_EQ(*it1, 6);
  ASSERT_EQ(*it2, 6);

  BoundedFIFO<uint8_t, TARGET_SIZE>::iterator it3 = circBuf.end() - 5;
  ASSERT_EQ(*it3, 6);

  // Reset it1, then test iterator difference operator.
  it1 = circBuf.begin();
  it2 = it1 + 5;
  ASSERT_EQ(it1 + 5, it2);
  ASSERT_EQ(it2 - it1, 5);

  // Check reference semantics.
  ASSERT_EQ(it1[0], 1);
  ASSERT_EQ(it1[5], *(it1 + 5));
  ASSERT_EQ(&it1[5], &( *(it1 + 5) ));
  ASSERT_EQ(it1[0], 1);

  // Check comparison operators.
  ASSERT_TRUE(it1 < it2);
  ASSERT_TRUE(it1 <= it2);
  ASSERT_FALSE(it1 >= it2);
  ASSERT_FALSE(it1 > it2);
  ASSERT_FALSE(it1 == it2);

  ASSERT_FALSE(it2 < it1);
  ASSERT_FALSE(it2 <= it1);
  ASSERT_TRUE(it2 >= it1);
  ASSERT_TRUE(it2 > it1);

  ASSERT_GT(it2 - it1, 0);
  ASSERT_FALSE(it1 < it1);

  // Since it1 < it2, make it2 < it3 and check it1 < it3
  it3 = it2 + 5;
  ASSERT_TRUE(it1 < it3);

  it1 += 5; // Test equal cases
  ASSERT_TRUE(it1 <= it2);
  ASSERT_TRUE(it1 >= it2);
  ASSERT_TRUE(it2 <= it1);
  ASSERT_TRUE(it2 >= it1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
