#pragma once

// C library headers
#include <stdio.h>
#include <unistd.h>

// C++ library headers
#include <atomic>
#include <random>

// Since gtest won't support timeouts... make our own
// Credit: https://github.com/google/googletest/issues/348#issuecomment-492810513
#include <future>
#define ASSERT_DURATION_LE(secs, stmt) \
  do { \
    std::promise<bool> completed; \
    auto stmt_future = completed.get_future(); \
    std::thread([&](std::promise<bool>& completed) { \
      { stmt; }\
      completed.set_value(true); \
    }, std::ref(completed)).detach(); \
    if(stmt_future.wait_for(std::chrono::seconds(secs)) == std::future_status::timeout) \
      GTEST_FATAL_FAILURE_("       timed out (> " #secs \
      " seconds). Check code for infinite loops"); \
  } while (0);

namespace TestUtils {

static int gStderrCopy = 0;
static int gStdoutCopy = 0;

// Prevent users from accidentally redirecting twice
static std::atomic<bool> gStderrRedir = false;
static std::atomic<bool> gStdoutRedir = false;

inline void StderrToBuf(char* buf, size_t size) {
  if (!gStderrRedir) {
    fflush(stderr);
    gStderrCopy = dup(STDERR_FILENO);
    [[maybe_unused]] auto tmp = freopen("/dev/null", "a+", stderr);
    if (setvbuf(stderr, buf, _IOFBF, size) != 0) {
      // Assume stdout is fine
      // NOTE: If users redirected both, may not see error message printed
      fprintf(stdout, "setvbuf failure upon redirection\n");
      return;
    }

    gStderrRedir = true;
  } else {
    // Assume stdout is fine
    fprintf(stdout, "stderr has already been redirected\n");
  }
}

inline void RestoreStderr() {
  if (gStderrRedir) {
    fflush(stderr);

    // Not clear why we need the following line, but we segfault if we don't.
    [[maybe_unused]] auto tmp = freopen("/dev/null", "a+", stderr);
    dup2(gStderrCopy, STDERR_FILENO);
    if (setvbuf(stderr, NULL, _IOFBF, 0) != 0) {
      // Assume stdout is fine
      // NOTE: If users redirected both, may not see error message printed
      fprintf(stdout, "setvbuf failure upon restore\n");
      return;
    }

    gStderrRedir = false;
  }
}

inline void StdoutToBuf(char* buf, size_t size) {
  if (!gStdoutRedir) {
    fflush(stdout);
    gStdoutCopy = dup(STDOUT_FILENO);
    [[maybe_unused]] auto tmp = freopen("/dev/null", "a+", stdout);
    if (setvbuf(stdout, buf, _IOFBF, size) != 0) {
      // Assume stderr is fine
      // NOTE: If users redirected both, may not see error message printed
      fprintf(stderr, "setvbuf failure upon redirection\n");
      return;
    }

    gStdoutRedir = true;
  } else {
    // Assume stderr is fine
    fprintf(stderr, "stdout has already been redirected\n");
  }
}

inline void RestoreStdout() {
  if (gStdoutRedir) {
    fflush(stdout);

    // Not clear why we need the following line, but we segfault if we don't.
    [[maybe_unused]] auto tmp = freopen("/dev/null", "a+", stdout);
    dup2(gStdoutCopy, STDOUT_FILENO);
    if (setvbuf(stdout, NULL, _IOFBF, 0) != 0) {
      // Assume stderr is fine
      // NOTE: If users redirected both, may not see error message printed
      fprintf(stderr, "setvbuf failure upon restore\n");
      return;
    }

    gStdoutRedir = false;
  }
}

// Fills 'buf' w/ 'bufSz' Bytes of random data
inline void FillRandBytes(uint8_t* buf, uint64_t bufSz) {
  static std::uniform_int_distribution<uint8_t> uint8Distr;

  static std::random_device rd;
  static std::mt19937_64 eng(rd());

  for (uint64_t i = 0; i < bufSz; i++) {
    buf[i] = uint8Distr(eng);
  }
}

} // TestUtils namespace
