#include "gtest/gtest.h"

// C++ libs
#include <string>

// C libs
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#include "i2c_utils.hpp"
#include "../log-utils/log_utils.hpp"
#include "../gtest-extras/test_utils.hpp"

using std::string;
using LogUtils::cppPrintf;
using TestUtils::FillRandBytes;
using namespace I2CUtils;

#define FIND_I2C_STUB_BUS_SHELL_CMD \
    "i2cdetect -l | grep 'SMBus stub driver' | awk '{print $1}'"
#define I2C_STUB_DEV_ADDR (0x03)
#define I2C_STUB_REG_ADDR (0x42)

// Get the bus name used by i2c-stub
string getI2CStubBus() {
  FILE* pOut = popen(FIND_I2C_STUB_BUS_SHELL_CMD, "r");
  if (pOut == NULL) {
    fprintf(stderr, "ERROR: Unable to open pipe to subshell\n");
    return "";
  }

  string busName;
  char tmpBuf[NAME_MAX] = {0};
  while (feof(pOut) == 0) {
    if (fgets(tmpBuf, NAME_MAX, pOut) == NULL) {
      continue;
    }

    // Strip newline from tmpBuf (one of fgets' stopping conditions)
    for (uint16_t i = 0; i < NAME_MAX && tmpBuf[i] != '\0'; i++) {
      if (tmpBuf[i] == '\n') {
        tmpBuf[i] = '\0';
      }
    }

    busName += tmpBuf;
  }

  pclose(pOut);
  return busName;
}

class I2CUtilsSMBusTest : public ::testing::Test {
  protected:
    I2CUtils::I2CDevice i2cDev_;
    string devPath_;

    void SetUp() override {
      // Get i2c-stub's bus name
      string busName = getI2CStubBus();
      ASSERT_GT(busName.size(), 0UL) <<
          cppPrintf("ERROR: Unable to obtain i2c-stub's bus name\n"
                    "       Ensure i2c-stub has been pre-loaded in the kernel"
                    " w/ chip address 0x%02hhX\n", I2C_STUB_DEV_ADDR);

      // Assume device is under /dev/ and ensure it's there
      devPath_ = "/dev/" + busName;
      bool devExists = (access(devPath_.c_str(), F_OK) == 0);
      ASSERT_EQ(devExists, true) <<
          cppPrintf("ERROR: Device %s does not exist\n", devPath_.c_str());
    }
};


TEST_F(I2CUtilsSMBusTest, OpenClose) {
  // Open bus using i2c-stub's bus name & do some basic sanity checks
  ASSERT_TRUE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, &i2cDev_));
  ASSERT_TRUE(i2cDev_.bus >= 3);
  ASSERT_TRUE(i2cDev_.addr == I2C_STUB_DEV_ADDR);

  // Close device & do some basic sanity checks
  i2c_smbus_close(&i2cDev_);
  ASSERT_TRUE(i2cDev_.bus == -1);
  ASSERT_TRUE(i2cDev_.addr == 0);
}

TEST_F(I2CUtilsSMBusTest, BadOpen) {
  // These tests will print to stderr, so redirect them to local buffer
  char buffer[BUFSIZ] = {0};
  TestUtils::StderrToBuf(buffer, BUFSIZ);

  // Attempt opening bus using a fake device name
  ASSERT_FALSE(i2c_smbus_open("/dev/fakeDev", I2C_STUB_DEV_ADDR, &i2cDev_));

  // Attempt opening bus using bad params
  ASSERT_FALSE(i2c_smbus_open("", I2C_STUB_DEV_ADDR, &i2cDev_));
  ASSERT_FALSE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, NULL));
  ASSERT_FALSE(i2c_smbus_open(devPath_.c_str(), 0x80, &i2cDev_));

  // Restores stderr
  TestUtils::RestoreStderr();
}

TEST_F(I2CUtilsSMBusTest, ReadWriteUint8) {
  // Open bus using i2c-stub's bus name
  ASSERT_TRUE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, &i2cDev_));

  // Attempt reading, should return >= 0
  int32_t ret = 0;
  ret = i2c_smbus_read_uint8(&i2cDev_, I2C_STUB_REG_ADDR);
  if (ret == -ENODEV) {
    fprintf(stderr, "ERROR: No device on bus %s with device address 0x%02X\n",
            devPath_.c_str(), I2C_STUB_DEV_ADDR);
  }
  ASSERT_TRUE(ret >= 0);

  // Increment read value and attempt to write a value
  uint8_t writeVal = (ret + 1) > 0xFF ? 0 : ((uint8_t)ret + 1);
  ret = i2c_smbus_write_uint8(&i2cDev_, I2C_STUB_REG_ADDR, writeVal);
  ASSERT_TRUE(ret == 0);

  // Re-read value to make sure it's the new value
  ret = i2c_smbus_read_uint8(&i2cDev_, I2C_STUB_REG_ADDR);
  ASSERT_TRUE(ret >= 0);
  ASSERT_TRUE(ret == (int32_t)writeVal);

  // Close device
  i2c_smbus_close(&i2cDev_);
  ASSERT_TRUE(i2cDev_.bus == -1);
  ASSERT_TRUE(i2cDev_.addr == 0);
}

TEST_F(I2CUtilsSMBusTest, BadReadWriteUint8) {
  // These tests will print to stderr, so redirect them to local buffer
  char buffer[BUFSIZ] = {0};
  TestUtils::StderrToBuf(buffer, BUFSIZ);

  // Attempt reading/writing w/o opening device
  ASSERT_TRUE(i2c_smbus_read_uint8(&i2cDev_, I2C_STUB_REG_ADDR) < 0);
  ASSERT_TRUE(i2c_smbus_write_uint8(&i2cDev_, I2C_STUB_REG_ADDR, 0x0) < 0);

  // Attmpt reading/writing w/ NULL device
  ASSERT_TRUE(i2c_smbus_read_uint8(NULL, I2C_STUB_REG_ADDR) < 0);
  ASSERT_TRUE(i2c_smbus_write_uint8(NULL, I2C_STUB_REG_ADDR, 0x0) < 0);

  // Restores stderr
  TestUtils::RestoreStderr();
}

TEST_F(I2CUtilsSMBusTest, ReadWriteUint16) {
  // Open bus using i2c-stub's bus name
  ASSERT_TRUE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, &i2cDev_));

  // Attempt reading, should return >= 0
  int32_t ret = 0;
  ret = i2c_smbus_read_uint16(&i2cDev_, I2C_STUB_REG_ADDR);
  ASSERT_TRUE(ret >= 0);

  // Increment read value and attempt to write a value
  uint16_t writeVal = (ret + 1) > 0xFFFF ? 0 : ((uint16_t)ret + 1);
  ret = i2c_smbus_write_uint16(&i2cDev_, I2C_STUB_REG_ADDR, writeVal);
  ASSERT_TRUE(ret == 0);

  // Re-read value to make sure it's the new value
  ret = i2c_smbus_read_uint16(&i2cDev_, I2C_STUB_REG_ADDR);
  ASSERT_TRUE(ret >= 0);
  ASSERT_TRUE(ret == (int32_t)writeVal);

  // Close device
  i2c_smbus_close(&i2cDev_);
  ASSERT_TRUE(i2cDev_.bus == -1);
  ASSERT_TRUE(i2cDev_.addr == 0);
}

TEST_F(I2CUtilsSMBusTest, BadReadWriteUint16) {
  // These tests will print to stderr, so redirect them to local buffer
  char buffer[BUFSIZ] = {0};
  TestUtils::StderrToBuf(buffer, BUFSIZ);

  // Attempt reading/writing w/o opening device
  ASSERT_TRUE(i2c_smbus_read_uint16(&i2cDev_, I2C_STUB_REG_ADDR) < 0);
  ASSERT_TRUE(i2c_smbus_write_uint16(&i2cDev_, I2C_STUB_REG_ADDR, 0x0) < 0);

  // Attmpt reading/writing w/ NULL device
  ASSERT_TRUE(i2c_smbus_read_uint16(NULL, I2C_STUB_REG_ADDR) < 0);
  ASSERT_TRUE(i2c_smbus_write_uint16(NULL, I2C_STUB_REG_ADDR, 0x0) < 0);

  // Restores stderr
  TestUtils::RestoreStderr();
}

TEST_F(I2CUtilsSMBusTest, ReadWriteBuffer) {
  // Open bus using i2c-stub's bus name
  ASSERT_TRUE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, &i2cDev_));

  // Cannot do SMBus read before an initial write
  // Generate random block of data & attempt to write values
  int32_t ret = 0;
  uint8_t writeBuf[I2C_SMBUS_BLOCK_MAX] = {0};
  FillRandBytes(writeBuf, I2C_SMBUS_BLOCK_MAX);

  ret = i2c_smbus_write_buffer(&i2cDev_, I2C_STUB_REG_ADDR,
                               writeBuf, I2C_SMBUS_BLOCK_MAX);
  if (ret == -EOPNOTSUPP) {
    fprintf(stderr, "ERROR: Ensure the I2C device being tested against "
            "supports SMBus block read/write commands.\n");
  }
  ASSERT_TRUE(ret == 0);

  // Re-read values to make sure they're identical
  uint8_t readBuf[I2C_SMBUS_BLOCK_MAX] = {0};
  ret = i2c_smbus_read_buffer(&i2cDev_, I2C_STUB_REG_ADDR, readBuf);
  ASSERT_TRUE(ret == I2C_SMBUS_BLOCK_MAX);
  ASSERT_TRUE(0 == memcmp(readBuf, writeBuf, I2C_SMBUS_BLOCK_MAX));

  // Close device
  i2c_smbus_close(&i2cDev_);
  ASSERT_TRUE(i2cDev_.bus == -1);
  ASSERT_TRUE(i2cDev_.addr == 0);
}

TEST_F(I2CUtilsSMBusTest, BadReadWriteBuffer) {
  // These tests will print to stderr, so redirect them to local buffer
  char buffer[BUFSIZ] = {0};
  TestUtils::StderrToBuf(buffer, BUFSIZ);

  int32_t ret = 0;
  uint8_t writeBuf[I2C_SMBUS_BLOCK_MAX] = {0};
  uint8_t readBuf[I2C_SMBUS_BLOCK_MAX] = {0};

  // Attempt reading/writing w/o opening device
  ret = i2c_smbus_write_buffer(&i2cDev_, I2C_STUB_REG_ADDR,
                               writeBuf, I2C_SMBUS_BLOCK_MAX);
  ASSERT_TRUE(ret < 0);

  ret = i2c_smbus_read_buffer(&i2cDev_, I2C_STUB_REG_ADDR, readBuf);
  ASSERT_TRUE(ret < 0);

  // Attmpt reading/writing w/ NULL device
  ret = i2c_smbus_write_buffer(NULL, I2C_STUB_REG_ADDR,
                               writeBuf, I2C_SMBUS_BLOCK_MAX);
  ASSERT_TRUE(ret < 0);

  ret = i2c_smbus_read_buffer(NULL, I2C_STUB_REG_ADDR, readBuf);
  ASSERT_TRUE(ret < 0);

  // Attempt reading/writing w/ NULL buffer
  ret = i2c_smbus_write_buffer(&i2cDev_, I2C_STUB_REG_ADDR,
                               NULL, I2C_SMBUS_BLOCK_MAX);
  ASSERT_TRUE(ret < 0);

  ret = i2c_smbus_read_buffer(&i2cDev_, I2C_STUB_REG_ADDR, NULL);
  ASSERT_TRUE(ret < 0);

  // Restores stderr
  TestUtils::RestoreStderr();

  // Open device & attempt to write more than I2C_SMBUS_BLOCK_MAX
  // Open bus using i2c-stub's bus name
  ASSERT_TRUE(i2c_smbus_open(devPath_.c_str(), I2C_STUB_DEV_ADDR, &i2cDev_));

  ret = i2c_smbus_write_buffer(&i2cDev_, I2C_STUB_REG_ADDR,
                               writeBuf, I2C_SMBUS_BLOCK_MAX * 2);
  ASSERT_TRUE(ret < 0);

  // Close device
  i2c_smbus_close(&i2cDev_);
  ASSERT_TRUE(i2cDev_.bus == -1);
  ASSERT_TRUE(i2cDev_.addr == 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
