#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

// C++ libs
#include <exception>
#include <thread>
#include <type_traits>

// C libs
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "log_utils.hpp"
#include "../gtest-extras/test_utils.hpp"

using std::string;
using std::exception;

using TestUtils::StderrToBuf;
using TestUtils::RestoreStderr;

using namespace LogUtils;

class LogUtilsTest : public ::testing::Test {
  protected:
    char buffer[BUFSIZ] = {0};
    Logger logger;

    void SetUp() override {
      // Create Logger
      logger = Logger();
    }
};

/*******************
 * Tests
 *******************/
TEST_F(LogUtilsTest, BasicLogging) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg = "hello world";
  logger(msg); // Use logger to "print" to 'buffer'

  // Restore stderr
  RestoreStderr();

  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content
  ASSERT_TRUE(bufMsg.find(msg) != string::npos);
}

TEST_F(LogUtilsTest, LogThrottling1) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg = "hello world";
  for (int i = 0; i < 10; i++) {
    logger(msg, 1s);
  }

  // Restore stderr
  RestoreStderr();

  // Should only see one instance of the message
  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find(msg);
  ASSERT_TRUE(startIdx != string::npos);

  ASSERT_TRUE(bufMsg.find(msg, startIdx + 1) == string::npos);
}

TEST_F(LogUtilsTest, LogThrottling2) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg = "hello world";
  logger(msg, 1s);
  std::this_thread::sleep_for(1s);
  logger(msg, 1s);

  // Restore stderr
  RestoreStderr();

  // Should only see two instances of the message
  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find(msg);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find(msg, startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  ASSERT_TRUE(bufMsg.find(msg, startIdx + 1) == string::npos);
}

TEST_F(LogUtilsTest, LogThrottling3) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg = "hello world";
  for (int i = 0; i < 10; i++) {
    logger(msg, 1s);
  }
  std::this_thread::sleep_for(1s);
  logger(msg, 1s);

  // Restore stderr
  RestoreStderr();

  // Should only see two instances of the message
  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find(msg);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find(msg, startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  ASSERT_TRUE(bufMsg.find(msg, startIdx + 1) == string::npos);

  // Should see "9 suppressed" message
  ASSERT_TRUE(bufMsg.find("9 suppressed") != string::npos);
}

TEST_F(LogUtilsTest, LogThrottleDiffMsgs1) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg1 = "hello world 1";
  string msg2 = "hello world 2";
  string msg3 = "hello world 3";

  for (int i = 0; i < 10; i++) {
    logger(msg1, 1s);
    logger(msg2, 1s);
    logger(msg3, 1s);
  }

  // Restore stderr
  RestoreStderr();

  // Should only see one instance of each message
  string bufMsg(buffer, BUFSIZ);

  size_t startIdx = bufMsg.find(msg1);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg1, startIdx + 1) == string::npos);

  startIdx = bufMsg.find(msg2);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg2, startIdx + 1) == string::npos);

  startIdx = bufMsg.find(msg3);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg3, startIdx + 1) == string::npos);
}

TEST_F(LogUtilsTest, LogThrottleDiffMsgs2) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  string msg1 = "hello world 1";
  string msg2 = "hello world 2";
  string msg3 = "hello world 3";

  for (int i = 0; i < 10; i++) {
    logger(msg1, 1s);
    logger(msg2, 1s);
    logger(msg3, 1s);
  }
  std::this_thread::sleep_for(1s);
  logger(msg1, 1s);
  logger(msg2, 1s);
  logger(msg3, 1s);

  // Restore stderr
  RestoreStderr();

  // Should see 2 instances of each message
  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find(msg1);
  ASSERT_TRUE(startIdx != string::npos);
  startIdx = bufMsg.find(msg1, startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg1, startIdx + 1) == string::npos);

  startIdx = bufMsg.find(msg2);
  ASSERT_TRUE(startIdx != string::npos);
  startIdx = bufMsg.find(msg2, startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg2, startIdx + 1) == string::npos);

  startIdx = bufMsg.find(msg3);
  ASSERT_TRUE(startIdx != string::npos);
  startIdx = bufMsg.find(msg3, startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);
  ASSERT_TRUE(bufMsg.find(msg3, startIdx + 1) == string::npos);

  // Should see exactly 3x "9 suppressed" messages
  startIdx = bufMsg.find("9 suppressed");
  ASSERT_TRUE(startIdx != string::npos);
  startIdx = bufMsg.find("9 suppressed", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);
  startIdx = bufMsg.find("9 suppressed", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find("9 suppressed", startIdx + 1);
  ASSERT_TRUE(startIdx == string::npos);
}

TEST(LogUtilsTestPrint, cppPrintf) {
  const char* cStr = "hello world";
  uint8_t one = 1;
  uint16_t two = 2;
  int three = 3;
  double pi = 3.14159;
  string msg = cppPrintf("%s %hhu %hu %d %.5lf\n", cStr, one, two, three, pi);

  ASSERT_EQ(msg, "hello world 1 2 3 3.14159\n");
}

TEST_F(LogUtilsTest, LogLevels) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  // Expected output format:
  //    [DEBUG] [1644855694.094570954] [testNode]: hello world
  //    [INFO] [1644855694.094605896] [testNode]: hello world
  //    [WARN] [1644855694.094611016] [testNode]: hello world
  //    [ERROR] [1644855694.094613754] [testNode]: hello world
  //    [FATAL] [1644855694.094616362] [testNode]: hello world
  string msg = "hello world";
  logger.setThreshold(Logger::DEBUG);
  logger(msg, Logger::DEBUG);
  logger(msg, Logger::INFO);
  logger(msg, Logger::WARN);
  logger(msg, Logger::ERROR);
  logger(msg, Logger::FATAL);
  logger(msg, Logger::NONE); // Shouldn't print anything

  // Restore stderr
  RestoreStderr();

  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find("[DEBUG]");
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find("[INFO]", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find("[WARN]", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find("[ERROR]", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find("[FATAL]", startIdx + 1);
  ASSERT_TRUE(startIdx != string::npos);

  // Make sure 'NONE' didn't print the message
  startIdx = bufMsg.find('\n', startIdx + 1);
  startIdx = bufMsg.find(msg, startIdx + 1);
  ASSERT_TRUE(startIdx == string::npos);
}

TEST_F(LogUtilsTest, SetThresholdFatal) {
  // Save old stderr FD & redirect to local buffer
  StderrToBuf(buffer, BUFSIZ);

  // Should only print once
  string msg = "hello world";
  logger.setThreshold(Logger::FATAL);
  logger(msg, Logger::DEBUG);
  logger(msg, Logger::INFO);
  logger(msg, Logger::WARN);
  logger(msg, Logger::ERROR);
  logger(msg, Logger::FATAL);
  logger(msg, Logger::NONE);

  // Restore stderr
  RestoreStderr();

  string bufMsg(buffer, BUFSIZ);
  //printf("%s\n", buffer); // Uncomment to see actual content

  size_t startIdx = bufMsg.find(msg);
  ASSERT_TRUE(startIdx != string::npos);

  startIdx = bufMsg.find(msg, startIdx + 1);
  ASSERT_TRUE(startIdx == string::npos);
}

/**
  * For lack of a better place right now, the type verification helpers for
  * cppPrintf (areFundamentalTypes and isFundamentalOrPointer) are in LogUtils,
  * so add tests here
  */
TEST(TypeVerification, isFundamentalOrPointer) {
  ASSERT_TRUE(isFundamentalOrPointer<int>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<int*>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<int&>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<const int>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<const int*>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<const int* const>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<void>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<void*>() == true);

  ASSERT_TRUE(isFundamentalOrPointer<string>() == false);
  ASSERT_TRUE(isFundamentalOrPointer<string&>() == false);
  ASSERT_TRUE(isFundamentalOrPointer<const string>() == false);

  ASSERT_TRUE(isFundamentalOrPointer<string*>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<const string*>() == true);
  ASSERT_TRUE(isFundamentalOrPointer<const string* const>() == true);

  enum TestEnum : uint8_t {};
  ASSERT_TRUE(isFundamentalOrPointer<TestEnum>() == false);
  ASSERT_TRUE(
      isFundamentalOrPointer<std::underlying_type_t<TestEnum>>() == true);
}

TEST(TypeVerification, areFundamentalOrPointer) {
  ASSERT_TRUE(areFundamentalOrPointer<int>() == true);
  ASSERT_TRUE(areFundamentalOrPointer<double>() == true);

  ASSERT_TRUE((areFundamentalOrPointer<int, double>() == true));
  ASSERT_TRUE((areFundamentalOrPointer<int, double, char>() == true));
  ASSERT_TRUE((areFundamentalOrPointer<int, double, char, void>() == true));
  ASSERT_TRUE(
      (areFundamentalOrPointer<const int, double*, char&, void>() == true));

  ASSERT_TRUE((areFundamentalOrPointer<int, string>() == false));
  ASSERT_TRUE((areFundamentalOrPointer<int, string&>() == false));
  ASSERT_TRUE((areFundamentalOrPointer<int, Logger>() == false));
  ASSERT_TRUE((areFundamentalOrPointer<int, const Logger&>() == false));

  ASSERT_TRUE((areFundamentalOrPointer<int, string*>() == true));
  ASSERT_TRUE((areFundamentalOrPointer<int, Logger*>() == true));
  ASSERT_TRUE((areFundamentalOrPointer<int, const string*>() == true));
  ASSERT_TRUE((areFundamentalOrPointer<int, const Logger*>() == true));

  // Test case with no params (e.g. cppPrintf() called w/ just a string)
  ASSERT_TRUE((areFundamentalOrPointer<>() == true));
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
