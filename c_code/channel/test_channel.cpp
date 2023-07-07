#include "channel.hpp"
#include "../gtest-extras/test_utils.hpp"

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

// C++ libs
#include <thread>
#include <vector>
#include <memory>

// C libs
#include <stdint.h>

using std::vector;
using std::unique_ptr;

void producer(int n, Channel<uint8_t>& chan, bool close = true) {
  for (int i = 0; i < n; i++) {
    chan.Put((uint8_t)(i % 255));
  }

  if (close) {
    chan.Close();
  }

  return;
}

bool TestChanSize1() {
  int numItems = 2000;
  Channel<uint8_t> byteChan(1);

  std::thread prod(producer, numItems, std::ref(byteChan), true);

  uint8_t val = 0;
  for (int i = 0; i < numItems; i++) {
    if (!byteChan.Get(val)) {
      fprintf(stderr, "In %s, Get() == false\n", __FUNCTION__);
      prod.join();
      return false;
    }

    EXPECT_TRUE(val == (uint8_t)(i % 255));
  }

  prod.join();
  return true;
}

bool TestChanSize1CloseMidway() {
  int numItems = 100;
  Channel<uint8_t> byteChan(1);
  std::thread prod(producer, numItems, std::ref(byteChan), true);

  uint8_t val = 0;
  for (int i = 0; i < numItems * 2; i++) {
    if (!byteChan.Get(val)) {
      prod.join();
      return false;
    }

    EXPECT_TRUE(val == (uint8_t)(i % 255));
  }

  // Shouldn't reach here
  fprintf(stderr, "In %s, more Get()s called than Put()s\n", __FUNCTION__);
  prod.join();
  return true;
}

bool TestChanSize1NoClose() {
  int numItems = 10;
  Channel<uint8_t> byteChan(1);

  // Launch producer that doesn't close at end
  std::thread prod(producer, numItems, std::ref(byteChan), false);

  uint8_t val = 0;
  for (int i = 0; i < numItems; i++) {
    if (!byteChan.Get(val)) {
      fprintf(stderr, "In %s, Get() == false\n", __FUNCTION__);
      prod.join();
      return false;
    }

    EXPECT_TRUE(val == (uint8_t)(i % 255));
  }

  byteChan.Get(val); // Should hang here

  prod.join();
  return true;
}

void producerVec(size_t n, size_t nBatch, Channel<uint8_t>& chan, bool close = true) {
  vector<uint8_t> block;
  for (size_t i = 0; i < n; i++) {
    block.push_back((uint8_t)(i % 255));
    if (block.size() == nBatch) {
      chan.Put(block);
      block.clear();
    }
  }

  // Put any remaining items into channel
  if (block.size()) {
    chan.Put(block);
  }

  if (close) {
    chan.Close();
  }

  return;
}

void producerArr(size_t n, size_t nBatch, Channel<uint8_t>& chan, bool close = true) {
  unique_ptr<uint8_t[]> block(new uint8_t[nBatch]);
  size_t i = 0;
  for (i = 0; i < n; i++) {
    block[i % nBatch] = (uint8_t)(i % 255);
    if ( ((i+1) % nBatch) == 0 ) {
      chan.Put(block.get(), nBatch);
    }
  }

  // Put any remaining items into channel
  if ((n % nBatch) != 0) {
    chan.Put(block.get(), n % nBatch);
  }

  if (close) {
    chan.Close();
  }

  return;
}

// If useVec == true, use vectors; else use arrays
//  - Only affects producer side (currently no Channel::Get() w/ arrays)
// If closeMidway == true, consumer (this func) attempts to Get() more items than produced
// If prodClose == true, producer closes after it finishes; else it doesn't
bool TestChanSize100(bool useVec = true,
                      bool closeMidway = false,
                      bool prodClose = true) {
  size_t numItems = 1000, batchSize = 50;
  Channel<uint8_t> byteChan(100);

  std::thread prod;
  if (useVec) {
    prod = std::thread(producerVec, numItems,
        batchSize, std::ref(byteChan), prodClose);
  } else {
    prod = std::thread(producerArr, numItems,
        batchSize, std::ref(byteChan), prodClose);
  }

  if (closeMidway) {
    numItems *= 2;
  }

  vector<uint8_t> valBuf;
  size_t nRead = 0;
  while (valBuf.size() < numItems) {
    if ((nRead = byteChan.Get(valBuf, batchSize)) == 0) {
      if (!closeMidway) {
        fprintf(stderr, "In %s(useVec = %s), Get() == 0\n",
                        __FUNCTION__, useVec ? "true" : "false");
        fprintf(stderr, "valBuf size is: %lu\n", valBuf.size());
      }
      prod.join();
      return false;
    }
  }

  if (valBuf.size() != numItems) {
    fprintf(stderr, "In %s(useVec = %s), valBuf.size() != numItems\n",
                      __FUNCTION__, useVec ? "true" : "false");
    prod.join();
    return false;
  }

  // Validate valBuf's contents
  for (size_t i = 0; i < numItems; i++) {
    EXPECT_TRUE(valBuf[i] == (uint8_t)(i % 255));
  }

  prod.join();
  return true;
}

TEST(ChannelTest, ChanSize1) {
  // Normal operation
  ASSERT_DURATION_LE(1, EXPECT_TRUE(TestChanSize1()));

  // Producer closes, consumer attempts Get()
  ASSERT_DURATION_LE(1, EXPECT_FALSE(TestChanSize1CloseMidway()));

  // Producer doesn't close, consumer attempts Get()
  EXPECT_FATAL_FAILURE(
      ASSERT_DURATION_LE(1, TestChanSize1NoClose()),
      "timed out");
}

TEST(ChannelTest, ChanSize100) {
  // Normal operation using vectors
  ASSERT_DURATION_LE(1, EXPECT_TRUE(TestChanSize100(true, false, true)));

  // Normal operation using array
  ASSERT_DURATION_LE(1, EXPECT_TRUE(TestChanSize100(false, false, true)));

  // Producer closes, consumer attempts Get(), using vectors
  ASSERT_DURATION_LE(1, EXPECT_FALSE(TestChanSize100(true, true, true)));

  // Producer closes, consumer attempts Get(), using arrays
  ASSERT_DURATION_LE(1, EXPECT_FALSE(TestChanSize100(false, true, true)));

  // Producer doesn't close, consumer attempts Get(), using vectors
  EXPECT_FATAL_FAILURE(
      ASSERT_DURATION_LE(1, EXPECT_TRUE(TestChanSize100(true, true, false))),
      "timed out");

  // Producer doesn't close, consumer attempts Get(), using vectors
  EXPECT_FATAL_FAILURE(
      ASSERT_DURATION_LE(1, EXPECT_TRUE(TestChanSize100(false, true, false))),
      "timed out");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
