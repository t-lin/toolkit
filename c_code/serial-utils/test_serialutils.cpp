#include "gtest/gtest.h"

// C++ libs
#include <string>

// C libs
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "serial_utils.hpp"
#include "../gtest-extras/test_utils.hpp"

using std::string;
using namespace SerialUtils;

// NOTE: Length of fake data should be <= BUFSIZ
constexpr const char* FAKE_DATA = "The quick red fox jumped over the lazy brown dog\n";
constexpr const uint64_t FAKE_DATA_LEN = __builtin_strlen(FAKE_DATA);
static_assert(BUFSIZ >= FAKE_DATA_LEN);

class SerialUtilsTest : public ::testing::Test {
  protected:
    FILE* tmpFile_ = nullptr;
    int devFD_ = -1;

    void SetUp() override {
      // Emulate a device via a file
      tmpFile_ = tmpfile();
      devFD_ = fileno(tmpFile_);
      ASSERT_TRUE(devFD_ > 2);
    }

    void TearDown() override {
      // Closing the FD associated w/ a file created by tmpfile() will
      // automatically remove it.
      ASSERT_TRUE(close(devFD_) == 0);
    }
};

// Use method under test to write, then use stdio API to read
TEST_F(SerialUtilsTest, WriteLen) {
  // Write data to file
  SerialOpRes written = writeLen(devFD_,
                                 reinterpret_cast<const uint8_t*>(FAKE_DATA),
                                 FAKE_DATA_LEN);
  ASSERT_TRUE(written.success);

  // Rewind file stream & read the file's contents
  rewind(tmpFile_);
  char readBuf[BUFSIZ] = {0};
  ASSERT_TRUE(FAKE_DATA_LEN == fread(readBuf, 0x1, FAKE_DATA_LEN, tmpFile_));

  // Fidelity check
  ASSERT_TRUE(0 == memcmp(readBuf, FAKE_DATA, FAKE_DATA_LEN));
}

TEST_F(SerialUtilsTest, WriteLen_BadFD) {
  int fakeDevFD = 1234; // Something infeasible

  // Silence stderr, as we expect it to be printed
  char errBuf[BUFSIZ] = {0};
  TestUtils::StderrToBuf(errBuf, BUFSIZ);

  // Write data to file
  SerialOpRes written = writeLen(fakeDevFD,
                                 reinterpret_cast<const uint8_t*>(FAKE_DATA),
                                 FAKE_DATA_LEN);
  ASSERT_FALSE(written.success);

  // Check if an error message was printed
  string bufMsg(errBuf, BUFSIZ);
  ASSERT_TRUE(bufMsg.find("ERROR: Unable to write to fd") != string::npos);

  // Restore stderr
  TestUtils::RestoreStderr();
}

// Use stdio API to write, then use method under test to read
TEST_F(SerialUtilsTest, ReadUntilLen) {
  // Pre-populate "device" w/ some data. Fill w/ 'BUFSIZ' bytes.
  char writeBuf[BUFSIZ] = {0};
  uint64_t nRemaining = 0;
  uint64_t nToWrite = 0;
  for (uint64_t i = 0; i < BUFSIZ; i += nToWrite) {
    nRemaining = BUFSIZ - i;
    nToWrite = nRemaining > FAKE_DATA_LEN ? FAKE_DATA_LEN : nRemaining;

    memcpy(writeBuf + i, FAKE_DATA, nToWrite);
  }
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Rewind file stream & read the file's contents
  rewind(tmpFile_);
  char readBuf[BUFSIZ] = {0};
  SerialOpRes read = readUntilLen(devFD_,
                                  reinterpret_cast<uint8_t*>(readBuf),
                                  BUFSIZ);
  ASSERT_TRUE(read.success);
  ASSERT_TRUE(read.nBytes == BUFSIZ);

  // Fidelity check
  ASSERT_TRUE(0 == memcmp(readBuf, writeBuf, BUFSIZ));
}

// Use stdio API to write, then use method under test to read
TEST_F(SerialUtilsTest, DiscardUntilDelimInclusive) {
  // Pre-populate "device" w/ some data. Fill w/ 'BUFSIZ' bytes.
  char writeBuf[BUFSIZ] = {0};
  uint64_t nRemaining = 0;
  uint64_t nToWrite = 0;
  for (uint64_t i = 0; i < BUFSIZ; i += nToWrite) {
    nRemaining = BUFSIZ - i;
    nToWrite = nRemaining > FAKE_DATA_LEN ? FAKE_DATA_LEN : nRemaining;

    memcpy(writeBuf + i, FAKE_DATA, nToWrite);
  }
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Search for delimiter; should return false
  rewind(tmpFile_);
  const char* delimSeq = "1234HelloWorld";
  uint64_t delimSeqLen = strlen(delimSeq);
  uint64_t delimInsIdx = 1234;

  SerialOpRes readRet = discardUntilDelimInclusive(
      devFD_, reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen, BUFSIZ);
  ASSERT_FALSE(readRet.success);

  // Insert a delimiter into the data before re-write to the "device"
  rewind(tmpFile_);
  memcpy(writeBuf + delimInsIdx, delimSeq, delimSeqLen);
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Rewind file stream & check if the delimiter was found
  rewind(tmpFile_);
  readRet = discardUntilDelimInclusive(
      devFD_, reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen, BUFSIZ);
  ASSERT_TRUE(readRet.success);

  // Rewind file stream & check if the delimiter was found w/ small maxDiscard
  // that is less than the point where the delimiter was inserted.
  rewind(tmpFile_);
  readRet = discardUntilDelimInclusive(
      devFD_, reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen, 100);
  ASSERT_FALSE(readRet.success);
}

TEST_F(SerialUtilsTest, ReadUntilDelimInclusive) {
  // Pre-populate "device" w/ some data. Fill w/ 'BUFSIZ' bytes.
  char writeBuf[BUFSIZ] = {0};
  uint64_t nRemaining = 0;
  uint64_t nToWrite = 0;
  for (uint64_t i = 0; i < BUFSIZ; i += nToWrite) {
    nRemaining = BUFSIZ - i;
    nToWrite = nRemaining > FAKE_DATA_LEN ? FAKE_DATA_LEN : nRemaining;

    memcpy(writeBuf + i, FAKE_DATA, nToWrite);
  }
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Search for delimiter; should return false
  rewind(tmpFile_);
  const char* delimSeq = "1234HelloWorld";
  uint64_t delimSeqLen = strlen(delimSeq);
  uint64_t delimInsIdx = 1234;

  char readBuf[BUFSIZ] = {0};
  SerialOpRes readRet = readUntilDelimInclusive(devFD_,
      reinterpret_cast<uint8_t*>(readBuf), BUFSIZ,
      reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen);
  ASSERT_TRUE(readRet.nBytes == BUFSIZ);
  ASSERT_FALSE(readRet.success);

  // Make sure what was read matches what was written
  ASSERT_TRUE(0 == memcmp(writeBuf, readBuf, BUFSIZ));

  // Insert a delimiter into the data before re-write to the "device"
  rewind(tmpFile_);
  memcpy(writeBuf + delimInsIdx, delimSeq, delimSeqLen);
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Rewind file stream & reset 'readBuf'
  // Check if the delimiter was found & count how much was read
  rewind(tmpFile_);
  memset(readBuf, 0x0, BUFSIZ);
  readRet = readUntilDelimInclusive(devFD_,
      reinterpret_cast<uint8_t*>(readBuf), BUFSIZ,
      reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen);
  ASSERT_TRUE(readRet.nBytes == delimInsIdx + delimSeqLen);

  // Reset readBuf & re-compare:
  //  - Entire buffer: *should not* match
  //  - Only up to 'nBytes': Should match
  ASSERT_FALSE(0 == memcmp(writeBuf, readBuf, BUFSIZ));
  ASSERT_TRUE(0 == memcmp(writeBuf, readBuf, readRet.nBytes));
}

// This case tests strings where delimiter sequence immediately follows
// a partial delimiter sequence
//  - e.g. If delimiter is "hello", then the buffer contains "hellhello"
TEST_F(SerialUtilsTest, OverlapDelimSequence) {
  // Pre-populate "device" w/ some data. Fill w/ 'BUFSIZ' bytes.
  char writeBuf[BUFSIZ] = {0};
  uint64_t nRemaining = 0;
  uint64_t nToWrite = 0;
  for (uint64_t i = 0; i < BUFSIZ; i += nToWrite) {
    nRemaining = BUFSIZ - i;
    nToWrite = nRemaining > FAKE_DATA_LEN ? FAKE_DATA_LEN : nRemaining;

    memcpy(writeBuf + i, FAKE_DATA, nToWrite);
  }
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Create overlapping delimiter sequence.
  // Insert a delimiter into the data somewhere in the middle.
  // Then insert again at the same index + strlen(delimSeq) - 1.
  const char* delimSeq = "1234HelloWorld";
  uint64_t delimSeqLen = strlen(delimSeq);
  uint64_t delimInsIdx = 1234;

  memcpy(writeBuf + delimInsIdx, delimSeq, delimSeqLen);
  memcpy(writeBuf + delimInsIdx + strlen(delimSeq) - 1, delimSeq, delimSeqLen);
  rewind(tmpFile_);
  ASSERT_TRUE(BUFSIZ == fwrite(writeBuf, 0x1, BUFSIZ, tmpFile_));

  // Rewind file stream.
  // Check if the delimiter was found & count how much was read
  char readBuf[BUFSIZ] = {0};
  rewind(tmpFile_);
  SerialOpRes readRet = readUntilDelimInclusive(devFD_,
      reinterpret_cast<uint8_t*>(readBuf), BUFSIZ,
      reinterpret_cast<const uint8_t*>(delimSeq), delimSeqLen);
  ASSERT_EQ(readRet.nBytes,
              delimInsIdx + delimSeqLen + strlen(delimSeq) - 1);
}

// TODO: Figure out how to do this one w/o root and in a container...
//TEST(SerialUtilsTest, OpenSerialDev) {
//}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
