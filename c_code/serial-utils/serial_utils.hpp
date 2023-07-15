#pragma once

// C library headers
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// C++ library headers
#include <vector>
#include <memory>

// Linux headers
#include <errno.h>        // Error integer and strerror() function
#include <fcntl.h>        // Contains file controls like O_RDWR
#include <sys/ioctl.h>    // For ioctl calls
#include <asm/termbits.h> // For termios2 and speed_t
#include <unistd.h>       // write(), read(), close()

namespace SerialUtils {

/**
 * @brief Structure for storing the result of serial operations
 */
typedef struct SerialOpRes {
  uint64_t nBytes = 0;
  bool success = false;
} SerialOpRes;

/**
 * @brief Reads from the device and fills the buffer with exactly 'len' bytes.
 *        This is a blocking call that only returns when 'len' has been read.
 *        We can concatenate subsequent partial reads because the tty driver
 *        in the kernel maintains a buffer between the device and our code,
 *        and read() may fetch from that buffer.
 *
 * @param devFD The device file descriptor to read from.
 * @param buf A pointer to the buffer to store data into.
 * @param len The amount of data (in Bytes) to read from the device. It is
 *            assumed that 'buf' has at least 'len' amount of memory.
 *
 * @return Returns a SerialOpRes object. Boolean 'SerialOpRes.success' is
 *         set to true if 'len' amount of data has been read, and false if
 *         an error occurred during a read operation. In both cases,
 *         'SerialOpRes.nBytes' represents the number of bytes successfully
 *         stored into 'buf'.
 */
inline
SerialOpRes readUntilLen(const int devFD,
                         uint8_t* const buf,
                         const uint64_t len) {
  if (devFD < 0) {
    fprintf(stderr, "ERROR: Invalid file descriptor %d\n", devFD);
    return {0, false};
  } else if (buf == NULL || len == 0) {
    fprintf(stderr, "ERROR: Invalid buffer (%p of length %lu)\n",
                                                (void*)buf, len);
    return {0, false};
  }

  uint64_t capLen = 0;
  ssize_t tmpLen = 0;
  do {
    tmpLen = read(devFD, buf + capLen, len - capLen);
    if (tmpLen == -1) {
      fprintf(stderr, "ERROR: Unable to read from fd %d; %s\n",
                                        devFD, strerror(errno));
      return {capLen, false};
    }

    capLen += (uint64_t)tmpLen;
  } while (capLen < len);

  return {capLen, true};
}

/**
 * @brief Reads from the device and fills the buffer until the given delimiter
 *        sequence has been encountered. This is a blocking call that only
 *        returns once the delimiter sequence has been found or the buffer
 *        is full. If the provided buffer 'buf' is NULL, the data will be
 *        discarded rather than stored, up to 'bufLen' bytes.
 *
 * @param devFD The device file descriptor to read from.
 * @param buf A pointer to the buffer to store data into. If NULL is provided,
 *            the read data will be discarded.
 * @param bufLen The capacity of 'buf'. If 'buf' is NULL, this will be the
 *               maximum amount of bytes read if the delimiter sequence is
 *               not encountered.
 * @param delimSeq A pointer to a delimiter sequence to look for.
 * @param delimLen The length of the delimiter sequence.
 *
 * @return Returns a SerialOpRes object. Boolean 'SerialOpRes.success' is
 *         set to true if the full delimiter sequence was encountered and
 *         stored into 'buf' (or discarded if 'buf' is NULL) prior to 'bufLen'
 *         bytes being read, and false otherwise. In both cases,
 *         'SerialOpRes.nBytes' represents the number of bytes stored into
 *         'buf' (or discarded if 'buf' is NULL).
 */
inline
SerialOpRes readUntilDelimInclusive(const int devFD,
                                    uint8_t* const buf,
                                    const uint64_t bufLen,
                                    const uint8_t* const delimSeq,
                                    const uint64_t delimLen) {
  if (devFD < 0) {
    fprintf(stderr, "ERROR: Invalid file descriptor %d\n", devFD);
    return {0, false};
  } else if (delimSeq == NULL || delimLen == 0) {
    fprintf(stderr, "ERROR: Invalid delimiter (%p with length %lu)\n",
                                            (void*)delimSeq, delimLen);
    return {0, false};
  } else if (delimLen > bufLen) { // Since delim will always be stored
    fprintf(stderr, "ERROR: Delimiter sequence length (%lu) > "
                      "buffer length (%lu)\n", delimLen, bufLen);
    return {0, false};
  } else if (buf == NULL && bufLen == 0) {
    fprintf(stderr, "ERROR: Invalid buffer (%p of length %lu)\n",
                                              (void*)buf, bufLen);
    return {0, false};
  }

  // If 'buf' == NULL, discard the data and treat 'bufLen' as maximum
  // number of bytes to read/discard.
  bool discardData = (buf == NULL);
  std::unique_ptr<uint8_t[]> tmpBuf(NULL); // Only used if discardData == true
  if (discardData) {
    // Create temporary buffer for read() to store data
    tmpBuf.reset(new uint8_t[delimLen]);

    if (tmpBuf.get() == NULL) {
      fprintf(stderr, "ERROR: Unable to allocate temporary buffer\n");
      return {0, false};
    }
  }

  // Set 'storeBuf' depending on whether we're storing or discarding
  uint8_t* storeBuf = discardData ? tmpBuf.get() : buf;

  // Now read data from the device until the delimeter sequence is found or
  // until 'bufLen' bytes has been read
  uint64_t delimIdx = 0;
  uint64_t nFilled = 0;
  uint64_t idx = 0;
  ssize_t nRead = 0;
  while (delimIdx < delimLen && nFilled < bufLen) {
    // If we're actually storing data, advance the 'storeBuf' pointer
    if (discardData == false) {
      storeBuf = buf + nFilled;
    }

    // Read up to 'delimLen' bytes at a time; read less if part of the
    // delimiter sequence was previously observed (i.e. delimIdx > 0)
    nRead = read(devFD, storeBuf, delimLen - delimIdx);

    if (nRead < 0) {
      fprintf(stderr, "ERROR: Unable to read from fd %d; %s\n",
                                        devFD, strerror(errno));
      return {nFilled, false};
    } else if (nRead == 0) {
      // Empty read
      continue;
    }

    for (idx = 0; idx < (uint64_t)nRead && (nFilled + idx) < bufLen; idx++) {
      if (storeBuf[idx] == delimSeq[delimIdx]) {
        delimIdx++;
      } else {
        // This index may be the start of the full delim sequence.
        delimIdx = (storeBuf[idx] == delimSeq[0]) ? 1 : 0;
      }
    }

    nFilled += idx;
  }

  if (delimIdx < delimLen && nFilled >= bufLen) {
    return {nFilled, false};
  }

  return {nFilled, true};
}

/**
 * @brief Reads from the device & discards data until the given delimiter
 *        sequence has been encountered, which will be discarded as well.
 *        This is a blocking call that only returns once the delimiter
 *        sequence has been found or 'maxDiscard' bytes has been read.
 *        This call is a short-hand for calling readUntilDelimInclusive()
 *        with a NULL buffer.
 *
 * @param devFD The device file descriptor to read from.
 * @param delimSeq A pointer to a delimiter sequence to look for.
 * @param delimLen The length of the delimiter sequence.
 * @param maxDiscard The maximum number of bytes to discard.
 *
 * @return Returns a SerialOpRes object. Boolean 'SerialOpRes.success' is
 *         set to true if the full delimiter sequence was encountered and
 *         discarded prior to 'maxDiscard' bytes being read, and false
 *         otherwise. In both cases, 'SerialOpRes.nBytes' represents the
 *         number of bytes discarded.
*/
inline
SerialOpRes discardUntilDelimInclusive(const int devFD,
                                       const uint8_t* const delimSeq,
                                       const uint64_t delimLen,
                                       const uint64_t maxDiscard) {
  if (delimLen > maxDiscard) {
    // Check this case here (instead of in readUntilDelimInclusive()) to
    // print a different error message.
    fprintf(stderr, "ERROR: Delimiter sequence length (%lu) > maximum "
                        "discard length (%lu)\n", delimLen, maxDiscard);
    return {0, false};
  }

  return readUntilDelimInclusive(devFD, 0, maxDiscard, delimSeq, delimLen);
}

/**
 * @brief Write 'len' bytes, read from 'buf', to the device associated with
 *        the file descriptor.
 *
 * @param devFD The device file descriptor to read from.
 * @param buf A pointer to the buffer to read data from.
 * @param len The amount of data (in Bytes) to read from the buffer and write
 *            to the device.
 *
 * @return Returns a SerialOpRes object. Boolean 'SerialOpRes.success' is
 *         set to true if 'len' amount of data has been written to the device,
 *         and false otherwise. In both cases, 'SerialOpRes.nBytes' represents
 *         the number of bytes successfully written to the device.
 */
inline
SerialOpRes writeLen(const int devFD,
                     const uint8_t* const buf,
                     const uint64_t len) {

  if (devFD < 0) {
    fprintf(stderr, "ERROR: Invalid file descriptor %d\n", devFD);
    return {0, false};
  } else if (buf == NULL || len == 0) {
    fprintf(stderr, "ERROR: Invalid buffer (%p of length %lu)\n",
                                                (void*)buf, len);
    return {0, false};
  }

  ssize_t nWritten = write(devFD, buf, len);
  if (nWritten < 0) {
    fprintf(stderr, "ERROR: Unable to write to fd %d; %s\n",
            devFD, strerror(errno));
    return {0, false};
  } else if ((uint64_t)nWritten < len) {
    fprintf(stderr, "ERROR: Incomplete write to fd %d; %ld/%lu written\n",
                                                    devFD, nWritten, len);
    return {(uint64_t)nWritten, false};
  }

  return {(uint64_t)nWritten, true};
}

/**
 * @brief Opens the named serial device and returns the file descriptor.
 *
 * @param devName A pointer to the name of the device.
 * @param baud_rate The baud rate to associate with the serial device.
 *
 * @return Returns -1 if any error occurs.
 */
inline
int openSerialDev(const char* const devName, speed_t baud_rate) {
  int devFD = open(devName, O_RDWR);
  if (devFD < 0) {
    fprintf(stderr, "ERROR: Unable to open serial device (%i): %s\n",
                                              errno, strerror(errno));
    return -1;
  }

  struct termios2 tty;
  if (0 != ioctl(devFD, TCGETS2, &tty)) {
    fprintf(stderr, "ERROR: %i from TCGETS2 ioctl: %s\n",
                                  errno, strerror(errno));
    return -1;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

  tty.c_cflag &= ~PARENB;  // Clear parity bit, disabling parity (most common)
  tty.c_cflag &= ~CSTOPB;  // Clear stop field, only one stop bit used in
                           // communication (most common)
  tty.c_cflag &= ~CSIZE;   // Clear all bits that set the data size
  tty.c_cflag |= CS8;      // 8 bits per byte (most common)
  tty.c_cflag &=
      ~CRTSCTS;  // Disable RTS/CTS hardware flow control (most common)
  tty.c_cflag |=
      CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines (CLOCAL = 1)

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO;    // Disable echo
  tty.c_lflag &= ~ECHOE;   // Disable erasure
  tty.c_lflag &= ~ECHONL;  // Disable new-line echo
  tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                   ICRNL);  // Disable any special handling of received bytes

  tty.c_oflag &= ~OPOST;  // Prevent special interpretation of output bytes
                          // (e.g. newline chars)
  tty.c_oflag &=
      ~ONLCR;  // Prevent conversion of newline to carriage return/line feed
  // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT
  // PRESENT ON LINUX) tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars
  // (0x004) in output (NOT PRESENT ON LINUX)

  tty.c_cc[VTIME] = 10;  // Wait for up to 1s (10 deciseconds) before returning
  tty.c_cc[VMIN] = 255;  // Wait for at least 255 bytes before returning

  // Set custom in/out baud rate
  tty.c_cflag &= ~CBAUD;
  tty.c_cflag |= CBAUDEX;
  tty.c_ispeed = baud_rate;
  tty.c_ospeed = baud_rate;

#pragma GCC diagnostic pop

  // Save tty settings, also checking for error
  if (0 != ioctl(devFD, TCSETS2, &tty)) {
    fprintf(stderr, "ERROR: %i from TCSETS2 ioctl: %s\n",
                                  errno, strerror(errno));
    return -1;
  }

  return devFD;
}

} // SerialUtils namespace
