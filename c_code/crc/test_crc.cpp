#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

// C++ libs
#include <iostream>
#include <random>
#include <ratio>
#include <string>
#include <memory>
#include <chrono>

// C libs
#include <stdint.h>

// Tins library
#include <tins/tins.h>

#include "../log-utils/log_utils.hpp"
#include "../gtest-extras/test_utils.hpp"

// Lib to be tested
#include "crc.hpp"

using std::string;
using std::unique_ptr;
using LogUtils::cppPrintf;
using CRCUtils::CRC;
using TestUtils::FillRandBytes;

using namespace std::chrono;
using namespace Tins;

const string TEST_STR1 = "hello world";
const string TEST_STR2 = "holy son of Mary";
const string TEST_STR3 = "The quick red fox jumped over the lazy brown dog.";


/**************
 * CRC8 Tests
 **************/
TEST(CRC8, BasicData) {
  const uint32_t BIT_WIDTH = 8;
  small_uint<BIT_WIDTH> crc8 = 0;

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                        TEST_STR1.size(), 0x0);
  EXPECT_TRUE(crc8 == 0x8F);

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                        TEST_STR2.size(), 0x0);
  EXPECT_TRUE(crc8 == 0xD9);

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                        TEST_STR3.size(), 0x0);
  EXPECT_TRUE(crc8 == 0x6D);
}

TEST(CRC8, DataWithInit) {
  const uint32_t BIT_WIDTH = 8;
  small_uint<BIT_WIDTH> crc8 = 0;
  small_uint<BIT_WIDTH> init = CRC_INIT & small_uint<BIT_WIDTH>::max_value;

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                        TEST_STR1.size(), init);
  EXPECT_TRUE(crc8 == 0x63);

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                        TEST_STR2.size(), init);
  EXPECT_TRUE(crc8 == 0x6A);

  crc8 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                        TEST_STR3.size(), init);
  EXPECT_TRUE(crc8 == 0x7E);
}

TEST(CRC8, Rand_1MB_Looped) {
  const uint32_t BIT_WIDTH = 8;

  // Declare volatile to avoid timing loop from being optimized out
  [[maybe_unused]] volatile small_uint<BIT_WIDTH>::repr_type crc8 = 0;

  const uint32_t BUF_SIZE = 1024 * 1024;
  unique_ptr<uint8_t> pData(new uint8_t[BUF_SIZE]);
  const uint32_t NUM_LOOPS = 1000;

  FillRandBytes(pData.get(), BUF_SIZE);

  auto start = steady_clock::now();
  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    crc8 = CRC<BIT_WIDTH>(pData.get(), BUF_SIZE, 0x0);
  }
  duration<double> elapsed = steady_clock::now() - start;

  std::cerr <<
      cppPrintf("Average time over %u bytes: %lf ms\n", BUF_SIZE,
                elapsed.count() * 1000 / NUM_LOOPS);
}

/***************
 * CRC14 Tests
 ***************/
TEST(CRC14, BasicData) {
  const uint32_t BIT_WIDTH = 14;
  small_uint<BIT_WIDTH> crc14 = 0;

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                         TEST_STR1.size(), 0x0);
  EXPECT_TRUE(crc14 == 0x11CB);

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                         TEST_STR2.size(), 0x0);
  EXPECT_TRUE(crc14 == 0x2B10);

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                         TEST_STR3.size(), 0x0);
  EXPECT_TRUE(crc14 == 0x3741);
}

TEST(CRC14, DataWithInit) {
  const uint32_t BIT_WIDTH = 14;
  small_uint<BIT_WIDTH> crc14 = 0;
  small_uint<BIT_WIDTH> init = CRC_INIT & small_uint<BIT_WIDTH>::max_value;

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                         TEST_STR1.size(), init);
  EXPECT_TRUE(crc14 == 0x88C);

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                         TEST_STR2.size(), init);
  EXPECT_TRUE(crc14 == 0x19A1);

  crc14 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                         TEST_STR3.size(), init);
  EXPECT_TRUE(crc14 == 0x3134);
}

TEST(CRC14, Rand_1MB_Looped) {
  const uint32_t BIT_WIDTH = 14;

  // Declare volatile to avoid timing loop from being optimized out
  [[maybe_unused]] volatile small_uint<BIT_WIDTH>::repr_type crc14 = 0;

  const uint32_t BUF_SIZE = 1024 * 1024;
  unique_ptr<uint8_t> pData(new uint8_t[BUF_SIZE]);
  const uint32_t NUM_LOOPS = 1000;

  FillRandBytes(pData.get(), BUF_SIZE);

  auto start = steady_clock::now();
  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    crc14 = CRC<BIT_WIDTH>(pData.get(), BUF_SIZE, 0x0);
  }
  duration<double> elapsed = steady_clock::now() - start;

  std::cerr <<
      cppPrintf("Average time over %u bytes: %lf ms\n", BUF_SIZE,
                elapsed.count() * 1000 / NUM_LOOPS);
}

/***************
 * CRC16 Tests
 ***************/
TEST(CRC16, BasicData_Poly_0x1021) {
  const uint32_t BIT_WIDTH = 16;
  small_uint<BIT_WIDTH> crc16 = 0;
  const uint64_t CRC_POLY = 0x1021U;

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR1.data(),
                                   TEST_STR1.size(), 0x0);
  EXPECT_TRUE(crc16 == 0x3BE4);

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR2.data(),
                                   TEST_STR2.size(), 0x0);
  EXPECT_TRUE(crc16 == 0x65D3);

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR3.data(),
                                   TEST_STR3.size(), 0x0);
  EXPECT_TRUE(crc16 == 0xAC9E);
}

TEST(CRC16, DataWithInit_Poly_0x1021) {
  const uint32_t BIT_WIDTH = 16;
  small_uint<BIT_WIDTH> crc16 = 0;
  const uint64_t CRC_POLY = 0x1021U;
  small_uint<BIT_WIDTH> init = CRC_INIT & small_uint<BIT_WIDTH>::max_value;

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR1.data(),
                                   TEST_STR1.size(), init);
  EXPECT_TRUE(crc16 == 0xA43B);

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR2.data(),
                                   TEST_STR2.size(), init);
  EXPECT_TRUE(crc16 == 0x5A4D);

  crc16 = CRC<BIT_WIDTH, CRC_POLY>((const uint8_t*)TEST_STR3.data(),
                                   TEST_STR3.size(), init);
  EXPECT_TRUE(crc16 == 0x5939);
}

TEST(CRC16, Rand_1MB_Looped_Poly_0x1021) {
  const uint32_t BIT_WIDTH = 16;
  const uint64_t CRC_POLY = 0x1021U;

  // Declare volatile to avoid timing loop from being optimized out
  [[maybe_unused]] volatile small_uint<BIT_WIDTH>::repr_type crc16 = 0;

  const uint32_t BUF_SIZE = 1024 * 1024;
  unique_ptr<uint8_t> pData(new uint8_t[BUF_SIZE]);
  const uint32_t NUM_LOOPS = 1000;

  FillRandBytes(pData.get(), BUF_SIZE);

  auto start = steady_clock::now();
  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    crc16 = CRC<BIT_WIDTH, CRC_POLY>(pData.get(), BUF_SIZE, 0x0);
  }
  duration<double> elapsed = steady_clock::now() - start;

  std::cerr <<
      cppPrintf("Average time over %u bytes: %lf ms\n", BUF_SIZE,
                elapsed.count() * 1000 / NUM_LOOPS);
}

/***************
 * CRC32 Tests
 ***************/
TEST(CRC32, BasicData) {
  const uint32_t BIT_WIDTH = 32;
  small_uint<BIT_WIDTH> crc32 = 0;

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                         TEST_STR1.size(), 0x0);
  EXPECT_TRUE(crc32 == 0x737AF2AE);

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                         TEST_STR2.size(), 0x0);
  EXPECT_TRUE(crc32 == 0xB8A4BEF1);

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                         TEST_STR3.size(), 0x0);
  EXPECT_TRUE(crc32 == 0x960FC54D);
}

TEST(CRC32, DataWithInit) {
  const uint32_t BIT_WIDTH = 32;
  small_uint<BIT_WIDTH> crc32 = 0;
  small_uint<BIT_WIDTH> init = CRC_INIT & small_uint<BIT_WIDTH>::max_value;

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR1.data(),
                         TEST_STR1.size(), init);
  EXPECT_TRUE(crc32 == 0xF60ED1DF);

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR2.data(),
                         TEST_STR2.size(), init);
  EXPECT_TRUE(crc32 == 0xF6038E8D);

  crc32 = CRC<BIT_WIDTH>((const uint8_t*)TEST_STR3.data(),
                         TEST_STR3.size(), init);
  EXPECT_TRUE(crc32 == 0xE854126F);
}

TEST(CRC32, Rand_1MB_Looped) {
  const uint32_t BIT_WIDTH = 32;

  // Declare volatile to avoid timing loop from being optimized out
  [[maybe_unused]] volatile small_uint<BIT_WIDTH>::repr_type crc32 = 0;

  const uint32_t BUF_SIZE = 1024 * 1024;
  unique_ptr<uint8_t> pData(new uint8_t[BUF_SIZE]);
  const uint32_t NUM_LOOPS = 1000;

  FillRandBytes(pData.get(), BUF_SIZE);

  auto start = steady_clock::now();
  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    crc32 = CRC<BIT_WIDTH>(pData.get(), BUF_SIZE, 0x0);
  }
  duration<double> elapsed = steady_clock::now() - start;

  std::cerr <<
      cppPrintf("Average time over %u bytes: %lf ms\n", BUF_SIZE,
                elapsed.count() * 1000 / NUM_LOOPS);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
