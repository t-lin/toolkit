#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

// C++ libs
#include <ios>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <random>

// C libs
#include <stdint.h>

// Lib to be tesed
#include "mplex_msg_frame.hpp"

// Max buffer size for MsgFrames
// 1472 = 1500 - 20 - 8; to emulate MTU of 1500
#define BUF_SIZE 1472

// Some preprocessor magic to silence warnings
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif

// Use 'pragma pack' here since there's a bug w/ '__attribute__((packed))' when
// passing members as references.
#pragma pack(push, 1)
typedef struct TestStruct {
  uint32_t a = 0;
  uint8_t b = 0;
  uint16_t c = 0;
  float d = 0;
  double e = 0;
  uint64_t f = 0;
} TestStruct;
#pragma pack(pop)
static_assert(sizeof(TestStruct) == 27);

using namespace std;

// Fills 'buf' w/ 'bufSz' Bytes of random data
void fillRandBytes(uint8_t* buf, uint64_t bufSz) {
  static uniform_int_distribution<uint8_t> uint8Distr;

  static random_device rd;
  static default_random_engine eng(rd());

  for (uint64_t i = 0; i < bufSz; i++) {
    buf[i] = uint8Distr(eng);
  }
}

TEST(MsgFramev0, BasicConstruction) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);

  // Create v0 MsgFrame
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame(buf.get(), BUF_SIZE);
  EXPECT_TRUE(msgFrame.id() == 0);
  EXPECT_TRUE(msgFrame.len() == 0);
  EXPECT_TRUE(msgFrame.isValid() == false);
  EXPECT_TRUE(msgFrame.processedSize() == sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(msgFrame.msgSize() == 0);

  // Test valid flags combinations
  vector<std::ios_base::openmode> goodFlags = {
    MplexOpMode::READ,
    MplexOpMode::WRITE,
    MplexOpMode::READ | MplexOpMode::WRITE,
  };

  for (auto& flag : goodFlags) {
    try {
      msgFrame = MplexMsgFrame<MsgFrameHeader_v0>(buf.get(), BUF_SIZE, flag);
    } catch (exception& exc) {
      FAIL() << "Valid flag (" + std::to_string(flag) + ") failed";
    }
  }
}

TEST(MsgFramev0, InvalidFlags) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);

  // Create v0 MsgFrame w/ invalid flags
  // User may force-feed other flags from ios_base, or some bad combo
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
  vector<std::ios_base::openmode> badFlags = {
    ~MplexOpMode::READ,
    ~MplexOpMode::WRITE,
    std::ios_base::openmode(0),
    std::ios_base::app,
    std::ios_base::ate,
    std::ios_base::binary,
    std::ios_base::trunc,
    std::ios_base::app | std::ios_base::binary,
    std::ios_base::ate | std::ios_base::binary,
    MplexOpMode::WRITE | std::ios_base::app,
    MplexOpMode::WRITE | std::ios_base::ate,
    MplexOpMode::WRITE | std::ios_base::binary,
    MplexOpMode::READ | std::ios_base::binary,
  };

  for (auto& flag : badFlags) {
    try {
      msgFrame = MplexMsgFrame<MsgFrameHeader_v0>(buf.get(), BUF_SIZE, flag);
      FAIL() << "Invalid flag (" + std::to_string(flag) + ") passed";
    } catch (exception& exc) {
      // Do nothing, this is expected
    }
  }
}

TEST(MsgFramev0, InvalidBuffer) {
  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
  EXPECT_TRUE(msgFrame.writeData(data.a) == false);
  EXPECT_TRUE(msgFrame.readData(data.a) == false);
  EXPECT_TRUE(msgFrame.writeHeader(10) == false);
  EXPECT_TRUE(msgFrame.isValid() == false);

  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  EXPECT_TRUE(msgFrame.copy(buf.get(), BUF_SIZE) == false);
}

TEST(MsgFramev0, HeaderOnlyFrame) {
  // Create underlying buffer for MsgFrame
  const uint16_t headerSize = sizeof(MsgFrameHeader_v0);
  unique_ptr<uint8_t[]> buf(new uint8_t[headerSize]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, headerSize);

  MplexMsgFrame<MsgFrameHeader_v0> msgFrame(buf.get(), headerSize);

  const MsgFrameHeader_v0::id_t id = 0b1010101;
  EXPECT_TRUE(msgFrame.writeHeader(id));

  EXPECT_TRUE(msgFrame.id() == id);
  EXPECT_TRUE(msgFrame.len() == 0);
  EXPECT_TRUE(msgFrame.isValid());
}

TEST(MsgFramev0, BasicReadWrite) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  // TODO: Add custom object that's not integral type or float
  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  // Create v0 MsgFrame & write data to buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame(buf.get(), BUF_SIZE);
  EXPECT_TRUE(msgFrame.writeData(data.a));
  EXPECT_TRUE(msgFrame.writeData(data.b));
  EXPECT_TRUE(msgFrame.writeData(data.c));
  EXPECT_TRUE(msgFrame.writeData(data.d));
  EXPECT_TRUE(msgFrame.writeData(data.e));
  EXPECT_TRUE(msgFrame.writeData(data.f));

  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));

  const MsgFrameHeader_v0::id_t id = 10;
  EXPECT_TRUE(msgFrame.writeHeader(id));
  EXPECT_TRUE(msgFrame.isValid());
  EXPECT_TRUE(msgFrame.msgSize() == msgFrame.processedSize());

  // Create new MsgFrame object w/ same buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame2(buf.get(), BUF_SIZE);
  EXPECT_TRUE(msgFrame2.isValid());
  EXPECT_TRUE(msgFrame2.id() == id);
  EXPECT_TRUE(msgFrame2.msgSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
  EXPECT_TRUE(msgFrame2.processedSize() ==
      sizeof(MsgFrameHeader_v0)); // Nothing read yet

  TestStruct data2 = {0};
  EXPECT_TRUE(msgFrame2.readData(data2.a));
  EXPECT_TRUE(msgFrame2.readData(data2.b));
  EXPECT_TRUE(msgFrame2.readData(data2.c));
  EXPECT_TRUE(msgFrame2.readData(data2.d));
  EXPECT_TRUE(msgFrame2.readData(data2.e));
  EXPECT_TRUE(msgFrame2.readData(data2.f));

  EXPECT_TRUE(memcmp(&data, &data2, sizeof(TestStruct)) == 0);
  EXPECT_TRUE(msgFrame2.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
}

TEST(MsgFramev0, ReadOnly) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  MplexMsgFrame<MsgFrameHeader_v0> msgFrame(buf.get(), BUF_SIZE,
                                            MplexOpMode::READ);

  // Try writing (should fail)
  EXPECT_FALSE(msgFrame.writeData(data.a));
  EXPECT_FALSE(msgFrame.writeData(data.b));
  EXPECT_FALSE(msgFrame.writeData(data.c));
  EXPECT_FALSE(msgFrame.writeData(data.d));
  EXPECT_FALSE(msgFrame.writeData(data.e));
  EXPECT_FALSE(msgFrame.writeData(data.f));

  // No data written, so only header
  EXPECT_TRUE(msgFrame.processedSize() == sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(msgFrame.isValid() == false);
  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
}

TEST(MsgFramev0, WriteOnly) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  TestStruct data = {0};

  MplexMsgFrame<MsgFrameHeader_v0> msgFrame(buf.get(), BUF_SIZE,
                                            MplexOpMode::WRITE);

  // Try reading (should fail)
  EXPECT_FALSE(msgFrame.readData(data.a));
  EXPECT_FALSE(msgFrame.readData(data.b));
  EXPECT_FALSE(msgFrame.readData(data.c));
  EXPECT_FALSE(msgFrame.readData(data.d));
  EXPECT_FALSE(msgFrame.readData(data.e));
  EXPECT_FALSE(msgFrame.readData(data.f));

  // No data read, so only header
  EXPECT_TRUE(msgFrame.processedSize() == sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(msgFrame.isValid() == false);
  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
}

TEST(MsgFramev0, ResetBuffer) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  // Create v0 MsgFrame & write data to buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
  EXPECT_TRUE(msgFrame.reset(buf.get(), BUF_SIZE));
  EXPECT_TRUE(msgFrame.writeData(data.a));
  EXPECT_TRUE(msgFrame.writeData(data.b));
  EXPECT_TRUE(msgFrame.writeData(data.c));
  EXPECT_TRUE(msgFrame.writeData(data.d));
  EXPECT_TRUE(msgFrame.writeData(data.e));
  EXPECT_TRUE(msgFrame.writeData(data.f));

  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));

  const MsgFrameHeader_v0::id_t id = 10;
  EXPECT_TRUE(msgFrame.writeHeader(id));
  EXPECT_TRUE(msgFrame.isValid());
  EXPECT_TRUE(msgFrame.msgSize() == msgFrame.processedSize());

  // Make copy of buffer, then use reset() on copied buffer for reading
  unique_ptr<uint8_t[]> buf2(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf2);
  memcpy(buf2.get(), buf.get(), BUF_SIZE);

  EXPECT_TRUE(msgFrame.reset(buf2.get(), BUF_SIZE));
  EXPECT_TRUE(msgFrame.isValid());
  EXPECT_TRUE(msgFrame.id() == id);
  EXPECT_TRUE(msgFrame.msgSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0)); // Nothing read yet

  TestStruct data2 = {0};
  EXPECT_TRUE(msgFrame.readData(data2.a));
  EXPECT_TRUE(msgFrame.readData(data2.b));
  EXPECT_TRUE(msgFrame.readData(data2.c));
  EXPECT_TRUE(msgFrame.readData(data2.d));
  EXPECT_TRUE(msgFrame.readData(data2.e));
  EXPECT_TRUE(msgFrame.readData(data2.f));

  EXPECT_TRUE(memcmp(&data, &data2, sizeof(TestStruct)) == 0);
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
}

TEST(MsgFramev0, CopyBuffer) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  // Create v0 MsgFrame & write data to buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
  EXPECT_TRUE(msgFrame.reset(buf.get(), BUF_SIZE));
  EXPECT_TRUE(msgFrame.writeData(data.a));
  EXPECT_TRUE(msgFrame.writeData(data.b));
  EXPECT_TRUE(msgFrame.writeData(data.c));
  EXPECT_TRUE(msgFrame.writeData(data.d));
  EXPECT_TRUE(msgFrame.writeData(data.e));
  EXPECT_TRUE(msgFrame.writeData(data.f));

  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));

  const MsgFrameHeader_v0::id_t id = 10;
  EXPECT_TRUE(msgFrame.writeHeader(id));
  EXPECT_TRUE(msgFrame.isValid());
  EXPECT_TRUE(msgFrame.msgSize() == msgFrame.processedSize());

  // Make another buffer for second MsgFrame, then use copy()
  // on first buffer and open for reading
  unique_ptr<uint8_t[]> buf2(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf2);
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame2(buf2.get(), BUF_SIZE);

  // Copy buffer, and do sanity + validation checks
  EXPECT_TRUE(msgFrame2.copy(buf.get(), msgFrame.msgSize()));
  EXPECT_TRUE(msgFrame.getBuf() != msgFrame2.getBuf());
  EXPECT_TRUE(memcmp(buf.get(), buf2.get(), msgFrame.msgSize()) == 0);
  EXPECT_TRUE(msgFrame2.isValid());
  EXPECT_TRUE(msgFrame2.id() == id);
  EXPECT_TRUE(msgFrame2.msgSize() == msgFrame.msgSize());
  EXPECT_TRUE(msgFrame2.processedSize() == sizeof(MsgFrameHeader_v0));

  // Read data back from copied buf
  TestStruct data2 = {0};
  EXPECT_TRUE(msgFrame2.readData(data2.a));
  EXPECT_TRUE(msgFrame2.readData(data2.b));
  EXPECT_TRUE(msgFrame2.readData(data2.c));
  EXPECT_TRUE(msgFrame2.readData(data2.d));
  EXPECT_TRUE(msgFrame2.readData(data2.e));
  EXPECT_TRUE(msgFrame2.readData(data2.f));

  EXPECT_TRUE(memcmp(&data, &data2, sizeof(TestStruct)) == 0);
  EXPECT_TRUE(msgFrame2.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
}

TEST(MsgFramev0, ByteStuffDestuff) {
  // Create underlying buffer for MsgFrame
  unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
  ASSERT_TRUE(buf);
  memset(buf.get(), 0x0, BUF_SIZE);

  // Add End of Message Group sequence to the beginning and end
  MsgFrameHeader_v0 endOfGroup;
  static_assert(sizeof(MsgFrameHeader_v0) < sizeof(uint64_t));

  // Create buffer for endOfGroup bytes, then use that to help fill data
  uint8_t* pEndOfGroup = reinterpret_cast<uint8_t*>(&endOfGroup);

  uint64_t endOfGroup64 = 0;
  for (uint8_t i = 0; i < sizeof(endOfGroup); i++) {
    endOfGroup64 = (endOfGroup64 << 8) |
                   reinterpret_cast<uint8_t*>(&endOfGroup)[i];
  }

  TestStruct data = {
    static_cast<uint32_t>((pEndOfGroup[0] << 24) |
                          (pEndOfGroup[1] << 16) |
                          (pEndOfGroup[2] << 8) |
                          pEndOfGroup[3]),
    pEndOfGroup[4], 30000, 4.1F, 5.2, endOfGroup64
  };

  // Create v0 MsgFrame & write data to buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
  EXPECT_TRUE(msgFrame.reset(buf.get(), BUF_SIZE));
  EXPECT_TRUE(msgFrame.writeData(data.a));
  EXPECT_TRUE(msgFrame.writeData(data.b));
  EXPECT_TRUE(msgFrame.writeData(data.c));
  EXPECT_TRUE(msgFrame.writeData(data.d));
  EXPECT_TRUE(msgFrame.writeData(data.e));
  EXPECT_TRUE(msgFrame.writeData(data.f));

  EXPECT_TRUE(msgFrame.msgSize() == 0); // Msg not yet valid
  EXPECT_TRUE(msgFrame.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));

#ifdef DEBUG
  cerr << "Before byte stuffing" << endl;
  msgFrame.printFrame();
#endif

  // Test byte-stuff method w/ invalid inputs

  EXPECT_TRUE(msgFrame.byteStuff(nullptr, 10) == -1);
  EXPECT_TRUE(msgFrame.byteStuff(
        reinterpret_cast<const uint8_t*>(&endOfGroup), 1) == -1);

  // Now byte-stuff the frame data properly
  int16_t bytesStuffed = msgFrame.byteStuff(
      reinterpret_cast<const uint8_t*>(&endOfGroup), sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(bytesStuffed == 2);

#ifdef DEBUG
  cerr << "After byte stuffing" << endl;
  msgFrame.printFrame();
#endif

  const MsgFrameHeader_v0::id_t id = 10;
  EXPECT_TRUE(msgFrame.writeHeader(id));
  EXPECT_TRUE(msgFrame.isValid());
  EXPECT_TRUE(msgFrame.msgSize() == msgFrame.processedSize());

  // Create new MsgFrame object w/ same buf
  MplexMsgFrame<MsgFrameHeader_v0> msgFrame2(buf.get(), BUF_SIZE);
  EXPECT_TRUE(msgFrame2.isValid());
  EXPECT_TRUE(msgFrame2.id() == id);
  EXPECT_TRUE(msgFrame2.msgSize() == sizeof(MsgFrameHeader_v0) +
                                     sizeof(TestStruct) +
                                     static_cast<uint16_t>(bytesStuffed));
  EXPECT_TRUE(msgFrame2.processedSize() ==
      sizeof(MsgFrameHeader_v0)); // Nothing read yet

  // Test byte-destuff method w/ invalid inputs
  EXPECT_TRUE(msgFrame.byteDestuff(nullptr, 10) == -1);
  EXPECT_TRUE(msgFrame.byteStuff(
        reinterpret_cast<const uint8_t*>(&endOfGroup), 1) == -1);

  // Now byte-destuff the frame data properly
  int16_t bytesDestuffed = msgFrame2.byteDestuff(
      reinterpret_cast<const uint8_t*>(&endOfGroup), sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(bytesDestuffed == bytesStuffed);

  TestStruct data2 = {0};
  EXPECT_TRUE(msgFrame2.readData(data2.a));
  EXPECT_TRUE(msgFrame2.readData(data2.b));
  EXPECT_TRUE(msgFrame2.readData(data2.c));
  EXPECT_TRUE(msgFrame2.readData(data2.d));
  EXPECT_TRUE(msgFrame2.readData(data2.e));
  EXPECT_TRUE(msgFrame2.readData(data2.f));

  EXPECT_TRUE(memcmp(&data, &data2, sizeof(TestStruct)) == 0);
  EXPECT_TRUE(msgFrame2.processedSize() ==
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
}

TEST(MsgFramev0, RandDelimSeqInData) {
  const uint32_t NUM_LOOPS = 5000000;
  const uint16_t AVOID_SEQ_LEN = 4;

  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    // Create underlying buffer for MsgFrame
    unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
    ASSERT_TRUE(buf);
    memset(buf.get(), 0x0, BUF_SIZE);

    // Randomly generate AVOID_SEQ_LEN Bytes as the "avoid" sequence
    // Fill buffer w/ this sequence at the beginning and end of 'data'
    uint8_t avoidSeq[AVOID_SEQ_LEN] = {0};
    fillRandBytes(avoidSeq, AVOID_SEQ_LEN);

    TestStruct data = {
      be32toh(*reinterpret_cast<uint32_t*>(avoidSeq)),
      200, 30000, 4.1F, 5.2,
      be32toh(*reinterpret_cast<uint32_t*>(avoidSeq))
    };

    // Create v0 MsgFrame & write data to buf
    MplexMsgFrame<MsgFrameHeader_v0> msgFrame;
    EXPECT_TRUE(msgFrame.reset(buf.get(), BUF_SIZE));
    EXPECT_TRUE(msgFrame.writeData(data.a));
    EXPECT_TRUE(msgFrame.writeData(data.b));
    EXPECT_TRUE(msgFrame.writeData(data.c));
    EXPECT_TRUE(msgFrame.writeData(data.d));
    EXPECT_TRUE(msgFrame.writeData(data.e));
    EXPECT_TRUE(msgFrame.writeData(data.f));

    uint8_t* msgData = const_cast<uint8_t*>(msgFrame.getBuf()) +
                                    sizeof(MsgFrameHeader_v0);
    ASSERT_TRUE(msgData != nullptr);
    memcpy(msgData, avoidSeq, AVOID_SEQ_LEN); // Beginning
    memcpy(msgData + sizeof(TestStruct) - AVOID_SEQ_LEN,
                                avoidSeq, AVOID_SEQ_LEN); // End

#ifdef DEBUG
    cerr << "Before byte stuffing" << endl;
    msgFrame.printFrame();
#endif

    // Stuff & write header of first frame and commit it
    int16_t bytesStuffed = msgFrame.byteStuff(avoidSeq, AVOID_SEQ_LEN);
    EXPECT_TRUE(bytesStuffed >= 2);

#ifdef DEBUG
    cerr << "After byte stuffing" << endl;
    msgFrame.printFrame();
#endif

    const MsgFrameHeader_v0::id_t id = 10;
    EXPECT_TRUE(msgFrame.writeHeader(id));
    EXPECT_TRUE(msgFrame.isValid());
    EXPECT_TRUE(msgFrame.msgSize() == msgFrame.processedSize());

    // Create new MsgFrame object w/ same buf
    MplexMsgFrame<MsgFrameHeader_v0> msgFrame2(buf.get(), BUF_SIZE);
    EXPECT_TRUE(msgFrame2.isValid());
    EXPECT_TRUE(msgFrame2.id() == id);
    EXPECT_TRUE(msgFrame2.msgSize() == sizeof(MsgFrameHeader_v0) +
                                       sizeof(TestStruct) +
                                       static_cast<uint16_t>(bytesStuffed));
    EXPECT_TRUE(msgFrame2.processedSize() ==
        sizeof(MsgFrameHeader_v0)); // Nothing read yet

    // Now byte-destuff the frame data properly
    int16_t bytesDestuffed = msgFrame2.byteDestuff(avoidSeq, AVOID_SEQ_LEN);
    EXPECT_TRUE(bytesDestuffed == bytesStuffed);

    TestStruct data2 = {0};
    EXPECT_TRUE(msgFrame2.readData(data2.a));
    EXPECT_TRUE(msgFrame2.readData(data2.b));
    EXPECT_TRUE(msgFrame2.readData(data2.c));
    EXPECT_TRUE(msgFrame2.readData(data2.d));
    EXPECT_TRUE(msgFrame2.readData(data2.e));
    EXPECT_TRUE(msgFrame2.readData(data2.f));

#ifdef DEBUG
    cerr << "After byte de-stuffing" << endl;
    msgFrame2.printFrame();
#endif

    EXPECT_TRUE(memcmp(&data, &data2, sizeof(TestStruct)) == 0);
    EXPECT_TRUE(msgFrame2.processedSize() ==
        sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
  }
}

//TEST(MsgFramev0, ReadWriteDiffEndian) {
//
//  // TODO: Write w/ specific endian, read w/ another
//}

// TODO: Test End of Group message

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
