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
#include "mplex_msg_group.hpp"

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

TestStruct randTestStruct() {
  static uniform_int_distribution<uint8_t> uint8Distr;
  static uniform_int_distribution<uint16_t> uint16Distr;
  static uniform_int_distribution<uint32_t> uint32Distr;
  static uniform_int_distribution<uint64_t> uint64Distr;
  static uniform_real_distribution<float>
      floatDistr(0, numeric_limits<float>::max());
  static uniform_real_distribution<double>
      doubleDistr(0, numeric_limits<double>::max());

  static random_device rd;
  static default_random_engine eng(rd());

  TestStruct data;
  data.a = uint32Distr(eng);
  data.b = uint8Distr(eng);
  data.c = uint16Distr(eng);
  data.d = floatDistr(eng);
  data.e = doubleDistr(eng);
  data.f = uint64Distr(eng);

  return data;
}

TEST(MsgGroupv0, Init_v0) {
  // Create message group w/ v0 messages
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup;

  EXPECT_TRUE(msgGroup.getBuf() != nullptr);
  EXPECT_TRUE(msgGroup.headerIsValid() == false);
  EXPECT_TRUE(msgGroup.calcGroupSize() == 0); // Header not valid
  EXPECT_TRUE(msgGroup.processedSize() ==
      sizeof(MsgGroupHeader_v0) + sizeof(MsgFrameHeader_v0));
}

// Write/serialize 2 structs into a buffer, then read them back.
TEST(MsgGroupv0, BasicReadWrite) {
  // Create message group w/ v0 messages in default WRITE mode
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup;

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};
  TestStruct data2 = {493827156, 100, 60000, 12.880519F, 16.336268, 9876543210};

  // Write data to the first frame
  auto pMsg = msgGroup.currFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->writeData(data.a));
  EXPECT_TRUE(pMsg->writeData(data.b));
  EXPECT_TRUE(pMsg->writeData(data.c));
  EXPECT_TRUE(pMsg->writeData(data.d));
  EXPECT_TRUE(pMsg->writeData(data.e));
  EXPECT_TRUE(pMsg->writeData(data.f));
  EXPECT_TRUE(pMsg->writeHeader(10));
  ASSERT_TRUE(pMsg->isValid());

  // Quick error test: object in WRITE mode should not allow reading or
  // searching for the next valid frame
  EXPECT_TRUE(pMsg->readData(data.a) == false);
  EXPECT_TRUE(msgGroup.nextValidFrame() == nullptr);

  uint16_t msgSize = pMsg->msgSize();
  EXPECT_TRUE(msgSize == sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));

  EXPECT_TRUE(msgGroup.headerIsValid() == false);

#ifdef DEBUG
  pMsg->printFrame();
#endif

  // Get next frame location
  // TODO: Create tests just for commitFrame?
  pMsg = msgGroup.commitFrame();
  ASSERT_TRUE(pMsg != nullptr);
  uint8_t* expectedLoc = msgGroup.getBuf() +
                         sizeof(MsgGroupHeader_v0) + msgSize;
  EXPECT_TRUE(pMsg->getBuf() == expectedLoc);

  // Write data2 to second frame
  EXPECT_TRUE(pMsg->writeData(data2.a));
  EXPECT_TRUE(pMsg->writeData(data2.b));
  EXPECT_TRUE(pMsg->writeData(data2.c));
  EXPECT_TRUE(pMsg->writeData(data2.d));
  EXPECT_TRUE(pMsg->writeData(data2.e));
  EXPECT_TRUE(pMsg->writeData(data2.f));
  EXPECT_TRUE(pMsg->writeHeader(100));
  ASSERT_TRUE(pMsg->isValid());
  EXPECT_TRUE(msgGroup.headerIsValid() == false);

#ifdef DEBUG
  pMsg->printFrame();
#endif
  pMsg = msgGroup.commitFrame(); // Finish w/ current frame

  // Finish creating Message Group
  EXPECT_TRUE(msgGroup.writeHeaderTrailer());
  EXPECT_TRUE(msgGroup.headerIsValid() == true);

  uint16_t expectedSize = sizeof(MsgGroupHeader_v0) +
      (2 * (sizeof(MsgFrameHeader_v0) + sizeof(TestStruct))) +
      sizeof(MsgFrameHeader_v0);
  ASSERT_TRUE(msgGroup.processedSize() == expectedSize);
  ASSERT_TRUE(msgGroup.calcGroupSize() == expectedSize);

#ifdef DEBUG
  msgGroup.printGroup();
#endif

  // Read data back from same buffer & store in 2 new structs
  // Creating a MsgGroup w/ an existing buffer puts it into READ mode
  TestStruct data3 = {0};
  TestStruct data4 = {0};
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup2(
      msgGroup.getBuf(), msgGroup.processedSize());

  // Sanity checks
  ASSERT_TRUE(msgGroup2.headerIsValid() == true);
  ASSERT_TRUE(msgGroup2.calcGroupSize() == msgGroup.processedSize());
  ASSERT_TRUE(msgGroup2.numFrames() == 2);
  ASSERT_TRUE(msgGroup2.processedSize() ==
      sizeof(MsgGroupHeader_v0) + sizeof(MsgFrameHeader_v0));

  // Get first frame and do some basic sanity checks
  pMsg = msgGroup2.currFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->isValid() == true);
  EXPECT_TRUE(pMsg->id() == 10);
  EXPECT_TRUE(pMsg->len() == sizeof(TestStruct));

  // Quick error test: object in READ mode should not allow writing or
  // commiting a frame
  EXPECT_TRUE(pMsg->writeData(data.a) == false);
  EXPECT_TRUE(msgGroup2.commitFrame() == nullptr);
  EXPECT_TRUE(msgGroup2.writeHeaderTrailer() == false);

  // Start reading data from the frame
  EXPECT_TRUE(pMsg->readData(data3.a));
  EXPECT_TRUE(pMsg->readData(data3.b));
  EXPECT_TRUE(pMsg->readData(data3.c));
  EXPECT_TRUE(pMsg->readData(data3.d));
  EXPECT_TRUE(pMsg->readData(data3.e));
  EXPECT_TRUE(pMsg->readData(data3.f));
  EXPECT_TRUE(memcmp(&data, &data3, sizeof(TestStruct)) == 0);

  // Get next frame and do some basic sanity checks
  pMsg = msgGroup2.nextValidFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->isValid() == true);
  EXPECT_TRUE(pMsg->id() == 100);
  EXPECT_TRUE(pMsg->len() == sizeof(TestStruct));

  EXPECT_TRUE(pMsg->readData(data4.a));
  EXPECT_TRUE(pMsg->readData(data4.b));
  EXPECT_TRUE(pMsg->readData(data4.c));
  EXPECT_TRUE(pMsg->readData(data4.d));
  EXPECT_TRUE(pMsg->readData(data4.e));
  EXPECT_TRUE(pMsg->readData(data4.f));
  EXPECT_TRUE(memcmp(&data2, &data4, sizeof(TestStruct)) == 0);

  // Getting next frame should return NULL
  pMsg = msgGroup2.nextValidFrame();
  EXPECT_TRUE(pMsg == nullptr);

  EXPECT_TRUE(msgGroup2.processedSize() == msgGroup.processedSize());
}

// Write/serialize n structs into a buffer, then read them back.
TEST(MsgGroupv0, RandomReadWrite) {
  const uint32_t NUM_LOOPS = 50000;
  const uint16_t NUM_MSGS = 50;
  static_assert(NUM_MSGS * sizeof(TestStruct) <= BUF_SIZE);

  // Repeat test NUM_LOOPS times
  for (uint32_t i = 0; i < NUM_LOOPS; i++) {
    // Create message group w/ v0 messages
    MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroupWrite;

    // Saved random TestStructs
    vector<TestStruct> rndDataList;

    // Write random TestStructs to message group
    auto pMsg = msgGroupWrite.currFrame();
    MsgFrameHeader_v0::id_t::repr_type loop = 0;
    for (loop = 0, pMsg = msgGroupWrite.currFrame();
          loop < NUM_MSGS && pMsg != nullptr;
          loop++, pMsg = msgGroupWrite.commitFrame()) {
      // Get random TestStruct
      TestStruct data = randTestStruct();
      rndDataList.push_back(data);

      EXPECT_TRUE(pMsg->writeData(data.a));
      EXPECT_TRUE(pMsg->writeData(data.b));
      EXPECT_TRUE(pMsg->writeData(data.c));
      EXPECT_TRUE(pMsg->writeData(data.d));
      EXPECT_TRUE(pMsg->writeData(data.e));
      EXPECT_TRUE(pMsg->writeData(data.f));

      EXPECT_TRUE(pMsg->writeHeader(loop));
      ASSERT_TRUE(pMsg->isValid());

#ifdef DEBUG
      pMsg->printFrame();
#endif
    }

    // Finish creating Message Group
    EXPECT_TRUE(msgGroupWrite.writeHeaderTrailer());
    EXPECT_TRUE(msgGroupWrite.headerIsValid() == true);

    // Sanity checks
    ASSERT_TRUE(loop == NUM_MSGS);
    uint16_t expectedSize = sizeof(MsgGroupHeader_v0) +
        (NUM_MSGS * (sizeof(MsgFrameHeader_v0) + sizeof(TestStruct))) +
        sizeof(MsgFrameHeader_v0);
    ASSERT_TRUE(msgGroupWrite.processedSize() == expectedSize);

    // Read random TestStructs from message group
    MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroupRead(
          msgGroupWrite.getBuf(), msgGroupWrite.processedSize());
    ASSERT_TRUE(msgGroupRead.headerIsValid() == true);
    ASSERT_TRUE(msgGroupRead.numFrames() == NUM_MSGS);

    loop = 0;
    pMsg = nullptr;
    for (loop = 0, pMsg = msgGroupRead.currFrame();
          loop < msgGroupRead.numFrames() && pMsg != nullptr;
          loop++, pMsg = msgGroupRead.nextValidFrame()) {
      // Sanity checks
      EXPECT_TRUE(pMsg->isValid() == true);
      EXPECT_TRUE(pMsg->id() == loop);
      EXPECT_TRUE(pMsg->len() == sizeof(TestStruct));

      TestStruct data;
      EXPECT_TRUE(pMsg->readData(data.a));
      EXPECT_TRUE(pMsg->readData(data.b));
      EXPECT_TRUE(pMsg->readData(data.c));
      EXPECT_TRUE(pMsg->readData(data.d));
      EXPECT_TRUE(pMsg->readData(data.e));
      EXPECT_TRUE(pMsg->readData(data.f));

      EXPECT_TRUE(memcmp(&data, &rndDataList[loop], sizeof(TestStruct)) == 0);
    }

    ASSERT_TRUE(pMsg == nullptr);
  }
}


// Write 2 structs into a buffer, corrupt a frame, then read them back.
TEST(MsgGroupv0, CorruptedFrame) {
  // Create message group w/ v0 messages
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup;

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};
  TestStruct data2 = {493827156, 100, 60000, 12.880519F, 16.336268, 9876543210};

  // Write data to the first frame
  auto pMsg = msgGroup.currFrame();
  ASSERT_TRUE(pMsg != nullptr);

  EXPECT_TRUE(pMsg->writeData(data.a));
  EXPECT_TRUE(pMsg->writeData(data.b));
  EXPECT_TRUE(pMsg->writeData(data.c));
  EXPECT_TRUE(pMsg->writeData(data.d));
  EXPECT_TRUE(pMsg->writeData(data.e));
  EXPECT_TRUE(pMsg->writeData(data.f));

  // Write header of first frame
  EXPECT_TRUE(pMsg->writeHeader(10));

  // Sanity checks
  uint16_t msgSize = pMsg->msgSize();
  EXPECT_TRUE(msgSize == sizeof(MsgFrameHeader_v0) + sizeof(TestStruct));
  ASSERT_TRUE(pMsg->isValid());
#ifdef DEBUG
  pMsg->printFrame();
#endif

  // Not done w/ group yet, so header shouldn't be valid
  EXPECT_TRUE(msgGroup.headerIsValid() == false);

  // Get next frame location
  pMsg = msgGroup.commitFrame();
  ASSERT_TRUE(pMsg != nullptr);
  uint8_t* expectedLoc = msgGroup.getBuf() +
                         sizeof(MsgGroupHeader_v0) + msgSize;
  EXPECT_TRUE(pMsg->getBuf() == expectedLoc);

  // Write data2 to second frame
  EXPECT_TRUE(pMsg->writeData(data2.a));
  EXPECT_TRUE(pMsg->writeData(data2.b));
  EXPECT_TRUE(pMsg->writeData(data2.c));
  EXPECT_TRUE(pMsg->writeData(data2.d));
  EXPECT_TRUE(pMsg->writeData(data2.e));
  EXPECT_TRUE(pMsg->writeData(data2.f));
  EXPECT_TRUE(pMsg->writeHeader(100));
  ASSERT_TRUE(pMsg->isValid());
  EXPECT_TRUE(msgGroup.headerIsValid() == false);

#ifdef DEBUG
  pMsg->printFrame();
#endif
  pMsg = msgGroup.commitFrame(); // Finish w/ current frame

  // Finish creating Message Group
  EXPECT_TRUE(msgGroup.writeHeaderTrailer());
  EXPECT_TRUE(msgGroup.headerIsValid() == true);

  uint16_t expectedSize = sizeof(MsgGroupHeader_v0) +
      (2 * (sizeof(MsgFrameHeader_v0) + sizeof(TestStruct))) +
      sizeof(MsgFrameHeader_v0);
  EXPECT_TRUE(msgGroup.processedSize() == expectedSize);

#ifdef DEBUG
  cerr << "Before corruption" << endl;
  msgGroup.printGroup();
#endif
  // Corrupt first frame
  uint8_t* pBuf = msgGroup.getBuf() +
    sizeof(MsgGroupHeader_v0) + sizeof(MsgFrameHeader_v0);
  *reinterpret_cast<int*>(pBuf) = 0;

#ifdef DEBUG
  cerr << "After corruption" << endl;
  msgGroup.printGroup();
#endif

  // Read data back from same buffer & store in new structs
  TestStruct data4 = {0};

  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup2(
      msgGroup.getBuf(), msgGroup.processedSize());
  ASSERT_TRUE(msgGroup2.headerIsValid() == true);
  ASSERT_TRUE(msgGroup2.calcGroupSize() == msgGroup.processedSize());
  ASSERT_TRUE(msgGroup2.numFrames() == 2);

  // Get first frame and do some basic sanity checks
  pMsg = msgGroup2.currFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->id() == 10);
  EXPECT_TRUE(pMsg->len() == sizeof(TestStruct));

  // Frame is corrupted, should be false
  EXPECT_TRUE(pMsg->isValid() == false);

  // Skip to next frame (since previous was corrupted) and perform sanity checks
  pMsg = msgGroup2.nextValidFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->isValid() == true);
  EXPECT_TRUE(pMsg->id() == 100);
  EXPECT_TRUE(pMsg->len() == sizeof(TestStruct));

  EXPECT_TRUE(pMsg->readData(data4.a));
  EXPECT_TRUE(pMsg->readData(data4.b));
  EXPECT_TRUE(pMsg->readData(data4.c));
  EXPECT_TRUE(pMsg->readData(data4.d));
  EXPECT_TRUE(pMsg->readData(data4.e));
  EXPECT_TRUE(pMsg->readData(data4.f));
  EXPECT_TRUE(memcmp(&data2, &data4, sizeof(TestStruct)) == 0);

  // Getting next frame should return NULL
  pMsg = msgGroup2.nextValidFrame();
  EXPECT_TRUE(pMsg == nullptr);
}

TEST(MsgGroupv0, ResetMsgGroup) {
  // Create message group w/ v0 messages
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup;

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  // Write data to the first frame
  auto pMsg = msgGroup.currFrame();
  ASSERT_TRUE(pMsg != nullptr);

  EXPECT_TRUE(pMsg->writeData(data.a));
  EXPECT_TRUE(pMsg->writeData(data.b));
  EXPECT_TRUE(pMsg->writeData(data.c));
  EXPECT_TRUE(pMsg->writeData(data.d));
  EXPECT_TRUE(pMsg->writeData(data.e));
  EXPECT_TRUE(pMsg->writeData(data.f));

  // Write header of first frame and commit it
  EXPECT_TRUE(pMsg->writeHeader(10));
  msgGroup.commitFrame();

  // Finish creating Message Group
  EXPECT_TRUE(msgGroup.writeHeaderTrailer());
  EXPECT_TRUE(msgGroup.headerIsValid() == true);
  EXPECT_TRUE(msgGroup.numFrames() == 1);
  uint16_t expectedSize = sizeof(MsgGroupHeader_v0) +
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct) +
      sizeof(MsgFrameHeader_v0);
  ASSERT_TRUE(msgGroup.processedSize() == expectedSize);

  // Now reset the group and perform basic sanity checks
  EXPECT_TRUE(msgGroup.reset());
  EXPECT_TRUE(msgGroup.headerIsValid() == false);
  EXPECT_TRUE(msgGroup.currFrame() == pMsg);
  EXPECT_TRUE(msgGroup.numFrames() == 0);
  ASSERT_TRUE(msgGroup.processedSize() ==
      sizeof(MsgGroupHeader_v0) + sizeof(MsgFrameHeader_v0));

  // Quick error test: reseting a MsgGroup puts it into WRITE mode; it
  // should not allow reading afterwards
  EXPECT_TRUE(pMsg->readData(data.a) == false);
}

TEST(MsgGroupv0, EndOfMsgInData) {
  // Create message group w/ v0 messages
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup;

  TestStruct data = {123456789, 200, 30000, 4.1F, 5.2, 6000};

  // Write data to the first frame
  auto pMsg = msgGroup.currFrame();
  ASSERT_TRUE(pMsg != nullptr);

  EXPECT_TRUE(pMsg->writeData(data.a));
  EXPECT_TRUE(pMsg->writeData(data.b));
  EXPECT_TRUE(pMsg->writeData(data.c));
  EXPECT_TRUE(pMsg->writeData(data.d));
  EXPECT_TRUE(pMsg->writeData(data.e));
  EXPECT_TRUE(pMsg->writeData(data.f));

  // Make data begin w/ same byte sequence as the EoMG
  MsgFrameHeader_v0 endOfGroup;
  uint8_t* msgData = const_cast<uint8_t*>(pMsg->getBuf()) +
                                  sizeof(MsgFrameHeader_v0);
  ASSERT_TRUE(msgData != nullptr);
  memcpy(msgData, &endOfGroup, sizeof(MsgFrameHeader_v0)); // Beginning

  // Stuff & write header of first frame and commit it
  // Stuffing should "escape" the EoMG sequence
  int16_t bytesStuffed = pMsg->byteStuff(
      reinterpret_cast<uint8_t*>(&endOfGroup), sizeof(MsgFrameHeader_v0));
  EXPECT_TRUE(bytesStuffed == 1);
  EXPECT_TRUE(pMsg->writeHeader(10));
  msgGroup.commitFrame();

  // Finish creating Message Group
  EXPECT_TRUE(msgGroup.writeHeaderTrailer());
  EXPECT_TRUE(msgGroup.headerIsValid() == true);
  EXPECT_TRUE(msgGroup.numFrames() == 1);
  uint16_t expectedSize = sizeof(MsgGroupHeader_v0) +
      sizeof(MsgFrameHeader_v0) + sizeof(TestStruct) +
      sizeof(MsgFrameHeader_v0) + static_cast<uint16_t>(bytesStuffed);
  ASSERT_TRUE(msgGroup.processedSize() == expectedSize);

#ifdef DEBUG
  cerr << "Before corruption" << endl;
  msgGroup.printGroup();
#endif
  // Corrupt first frame, forcing later calls to calcGroupSize() and
  // nextValidFrame(), that scan over the buffer, to potentially misinterpret
  // it as an EoMG frame.
  uint8_t* pBuf = msgGroup.getBuf() +
    sizeof(MsgGroupHeader_v0) + (sizeof(MsgFrameHeader_v0) * 3);
  *reinterpret_cast<int*>(pBuf) = 0;

#ifdef DEBUG
  cerr << "After corruption" << endl;
  msgGroup.printGroup();
#endif

  // Read data back from same buffer & should have no valid frames
  MplexMsgGroup<MsgGroupHeader_v0, MsgFrameHeader_v0> msgGroup2(
      msgGroup.getBuf(), msgGroup.processedSize());
  ASSERT_TRUE(msgGroup2.headerIsValid() == true);
  ASSERT_TRUE(msgGroup2.numFrames() == 1);

  // If stuffing failed, the following test should be false
  ASSERT_TRUE(msgGroup2.calcGroupSize() == msgGroup.processedSize());

  // Get first frame and do some basic sanity checks
  pMsg = msgGroup2.currFrame();
  ASSERT_TRUE(pMsg != nullptr);
  EXPECT_TRUE(pMsg->id() == 10);
  EXPECT_TRUE(pMsg->len() == sizeof(TestStruct) +
                             static_cast<uint16_t>(bytesStuffed));

  // Frame is corrupted, should be false
  EXPECT_TRUE(pMsg->isValid() == false);

  // Skip to next frame (since previous was corrupted) and perform sanity checks
  pMsg = msgGroup2.nextValidFrame();
  ASSERT_TRUE(pMsg == nullptr);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
