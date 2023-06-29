#ifndef MPLEX_MSG_FRAME_H
#define MPLEX_MSG_FRAME_H

// C headers
#include <stdint.h>
#include <asm/byteorder.h>
#include <endian.h>
#include <string.h>

// C++ headers
#include <iostream>
#include <exception>
#include <type_traits>
#include <vector>

// libtins
#include <tins/tins.h>
#include <tins/small_uint.h>
#include <tins/memory_helpers.h>

#include "../crc/crc.hpp"
#include "headers/frame_headers.hpp"

#if (__FLOAT_WORD_ORDER == __LITTLE_ENDIAN)
// Helper functions
inline float reverseFloat(float val) {
  uint32_t* pVal = reinterpret_cast<uint32_t*>(&val);
  uint32_t tmp = __builtin_bswap32(*pVal);

  float* pFloat = reinterpret_cast<float*>(&tmp);
  return *pFloat;
}

inline double reverseDouble(double val) {
  uint64_t* pVal = reinterpret_cast<uint64_t*>(&val);
  uint64_t tmp = __builtin_bswap64(*pVal);

  double* pDouble = reinterpret_cast<double*>(&tmp);
  return *pDouble;
}
#endif

// Flags that specify the operational mode of MplexMsgFrames
// TODO: Replace w/ a class object?
namespace MplexOpMode {
  typedef std::ios_base::openmode opmode;

  static const opmode READ = std::ios_base::in;
  static const opmode WRITE = std::ios_base::out;
}

// Empty base class to enable polymorphic pointers
class MplexMsgFrameBase {
};

/*
 * MplexMsgFrame class
 * Simply a wrapper around an existing underlying buffer that holds a
 * serialized Message Frame. Can be used to write/serialize data to the
 * underlying buffer, or read from it.
 *
 * Inspired by fstream & libtins' OutputMemoryStream and InputMemoryStream.
 */
template <typename MsgFrameHeader>
class MplexMsgFrame : MplexMsgFrameBase {
  public:
    // Header-dependent types
    typedef Tins::small_uint<MsgFrameHeader::ID_WIDTH> id_t;
    typedef Tins::small_uint<MsgFrameHeader::LEN_WIDTH> len_t;
    typedef Tins::small_uint<MsgFrameHeader::CRC_WIDTH> crc_t;

    static const uint8_t MAGIC_NUMBER = MsgFrameHeader::MAGIC_NUMBER;

  private:
    // Pointer to entire frame buffer & its size
    const uint8_t* rawBuf_ = nullptr;
    uint16_t rawBufSize_ = 0;

    // Header structure & pointer to data portion of buffer
    // NOTE: Basic types in header (non-bitfields) must be stored in
    //       big-endian format. Must convert to/from host order when
    //       writing to / reading from the header.
    MsgFrameHeader header_;
    uint8_t* data_ = nullptr;

    // Position in data_, relative to the beginning, for reading/writing
    uint16_t rwPos_ = 0;

    // Operational mode
    MplexOpMode::opmode mode_ = MplexOpMode::READ | MplexOpMode::WRITE;

    // Memory helpers
    // Initialize here so default constructor can be used
    Tins::Memory::OutputMemoryStream outStream_ =
        Tins::Memory::OutputMemoryStream(nullptr, 0);
    Tins::Memory::InputMemoryStream inStream_ =
        Tins::Memory::InputMemoryStream(nullptr, 0);

    /*
     * Calls CRC() w/ an initial value based on the CRC bit-width. This is
     * to prevent 0 from being the default CRC and prevents arbitrary-length
     * all-0 data from having the same CRC.
     */
    crc_t calcCRC_(const uint8_t* buf, uint64_t sz) const {
      crc_t init = CRC_INIT & crc_t::max_value;
      return CRCUtils::CRC<MsgFrameHeader::CRC_WIDTH>(buf, sz, init);
    }

  public:
    // Useful header-dependent constants
    static const vers_t VERS = MsgFrameHeader::VERS;
    static const uint16_t MAX_DATA = len_t::max_value;
    static const uint16_t MIN_SIZE = sizeof(MsgFrameHeader);
    static const uint16_t MAX_SIZE = sizeof(MsgFrameHeader) + MAX_DATA;

    // Default constructor
    MplexMsgFrame() {}

    /*
     * Initialization constructor: simply wraps a buffer pointer 'buf' with
     * buffer size 'sz'. Prepares the object for writing/serializing data to,
     * or reading from, the underlying buffer.
     */
    MplexMsgFrame(uint8_t* buf, uint16_t sz,
        MplexOpMode::opmode mode = MplexOpMode::READ | MplexOpMode::WRITE) {

      if (mode == 0 || mode & ~(MplexOpMode::READ | MplexOpMode::WRITE)) {
        throw std::invalid_argument("Unsupported operational mode flag(s)");
      }

      if (sz < sizeof(MsgFrameHeader)) {
        throw std::invalid_argument("Buffer size < header size");
      } else if (sz > MAX_SIZE) {
        throw std::invalid_argument("Buffer size > max MsgFrame size");
      }

      if (this->reset(buf, sz, mode) == false) {
        throw std::logic_error("Unable to set MsgFrame object");
      }
    }

    // TODO: Create copy constructor?

    /**
     * @brief  Similar to initialization constructor, allows re-use of object.
     * Differs from constructor in that it returns true/false when an error
     * is encountered, rather than throwing an exception.
     *
     * Wraps buffer pointer 'buf', with buffer size 'sz', and prepares the
     * object for read/write operations. The internal stream position will
     * be reset to 0.
     *
     * @param buf Pointer to buffer to be copied.
     * @param sz Size of the buffer to copy from.
     * @param mode Operational mode for this MsgFrame object.
     *
     * @return Returns true if reset completed successfully, false otherwise.
     */
    bool reset(uint8_t* buf, uint16_t sz,
        MplexOpMode::opmode mode = MplexOpMode::READ | MplexOpMode::WRITE) {

      // TODO: Refactor error checking w/ unified function
      if (mode == 0 || mode & ~(MplexOpMode::READ | MplexOpMode::WRITE)) {
        // TODO: Replace w/ log
        std::cerr << "Invalid operational mode specified" << std::endl;
        return false;
      }

      if (buf == nullptr) {
        // TODO: Replace w/ log
        std::cerr << "Invalid operational mode specified" << std::endl;
        return false;
      }

      if (sz < sizeof(MsgFrameHeader)) {
        // TODO: Replace w/ log
        std::cerr << "Buffer size > max MsgFrame size" << std::endl;
        return false;
      } else if (sz > MAX_SIZE) {
        // TODO: Replace w/ log
        std::cerr << "Buffer size > max MsgFrame size" << std::endl;
        return false;
      }

      rawBuf_ = buf;
      rawBufSize_ = sz;
      data_ = sz == sizeof(MsgFrameHeader) ? nullptr :
                                             buf + sizeof(MsgFrameHeader);
      rwPos_ = 0;
      mode_ = mode;

      // Assume buf holds a serialized Message Frame & load the header.
      // Afterwards, inStream_'s position will be the start of data_.
      inStream_ = Tins::Memory::InputMemoryStream(buf, sz);
      inStream_.read<MsgFrameHeader>(header_);

      // Set outStream_'s position to the start of data_
      outStream_ = Tins::Memory::OutputMemoryStream(
          data_, sz - sizeof(MsgFrameHeader));

      return true;
    }

    /**
     * @brief Copies a serialized Message Frame from 'buf', with content size
     * 'sz', into the current underlying buffer from the beginning of the
     * buffer. This operation will thus override any existing data.
     *
     * The new header is then loaded from the buffer and the object is prepared
     * for read/write operations with the internal stream position reset to 0.
     * Note that 'sz' must not exceed the size of the underlying buffer.
     *
     * @param buf Pointer to buffer to be copied.
     * @param sz Size of the buffer to copy from.
     * @param mode Operational mode for this MsgFrame object.
     *
     * @return Returns true if copy completed successfully, false otherwise.
     */
    bool copy(uint8_t* buf, uint16_t sz,
        MplexOpMode::opmode mode = MplexOpMode::READ | MplexOpMode::WRITE) {
      if (sz > rawBufSize_ || rawBuf_ == nullptr) {
        return false;
      }

      memcpy((void*)rawBuf_, (void*)buf, sz);
      return this->reset(const_cast<uint8_t*>(rawBuf_), rawBufSize_, mode);
    }

    // Checks header to see if this is an End of Message Group frame
    bool isEndOfMsgGroup() const {
      return header_.magic() == MsgFrameHeader::MAGIC_NUMBER &&
             header_.id() == MsgFrameHeader::END_OF_GROUP_ID &&
             header_.len() == 0 &&
             header_.crc() == 0;
    }

    /**
     * @brief (Re-)generate a Message Header, with the given message ID, and
     *        write it to the underlying buffer. The length written will be
     *        the data length written/read so far.
     *
     * @param id is the message ID.
     *
     * @return Returns true if completed successfully, otherwise returns false.
     */
    bool writeHeader(id_t id) {
      uint32_t dataLen = rwPos_;
      if (dataLen > MAX_DATA) {
        // TODO: Replace w/ log
        std::cerr << "ERROR: Data length (" << dataLen <<
            ") > than max data length (" << MAX_DATA << ")\n";
        return false;
      } else if (rawBuf_ == nullptr) {
        return false;
      }

      // The CRC field in the header is calculated over both header + data,
      // with the CRC field initially set to 0. Then re-update the CRC field.
      header_.setMagic();
      header_.id(id);
      header_.len(dataLen & len_t::max_value);
      header_.crc(0);
      memcpy((void*)rawBuf_, (void*)&header_, sizeof(MsgFrameHeader));

      uint16_t procSize = this->processedSize();
      try {
        header_.crc(calcCRC_(rawBuf_, procSize));
      } catch (std::exception& exc) {
        // TODO: Replace w/ log
        std::cerr << "ERROR: Unable to write CRC; " << exc.what() << std::endl;
        return false;
      }
      memcpy((void*)rawBuf_, (void*)&header_, sizeof(MsgFrameHeader));

      return true;
    }

    /**
     * @brief Byte-stuffs the data portion of the message to avoid duplicates
     * of 'avoidSeq', a sequence of characters with length 'seqSz'. This
     * operation finds sub-sequences consisting of the first ('seqSz' - 1)
     * bytes of 'avoidSeq' and then stuffing an "escape" byte, defined as the
     * bit-wise inverse of the last byte of 'avoidSeq', immediately afterwards.
     *
     * This procedure takes inspiration from HDLC bit-stuffing, but generalized
     * for delimiters consisting of a sequences of bytes.
     *
     * Notes:
     *  1) This operation may increase the length of the data portion of the
     *     data payload. Users intending to transmit the frame post-stuffing
     *     should call writeHeader() to correct the CRC and length fields.
     *  2) Users should not call this multiple times. If this is done for some
     *     valid reason (e.g. passing data through different underlying
     *     protocols w/ different delimiters), users should take care to
     *     de-stuff in the reverse order.
     *
     * @param avoidSeq Pointer to a sequence of characters to be avoided.
     * @param seqSz Length of 'avoidSeq'. Must be at least 2.
     *
     * @return Returns the number of extra bytes stuffed into the data.
     *         Returns -1 if any of the following are true:
     *          - 'avoidSeq' is NULL or 'seqSz' < 2
     *          - The number of stuffed characters required will exceed the
     *            size of the undelrying buffer.
     */
    int16_t byteStuff(const uint8_t* avoidSeq, uint16_t seqSz) {
      if (avoidSeq == nullptr || seqSz < 2) {
        return -1;
      }

      // Count the number of necessary extra bytes to stuff. i.e. Count the
      // number of occurrences of the first ('seqSz' - 1) bytes of 'avoidSeq'.
      uint8_t* startSearch = data_;
      void* seqStart = nullptr;
      uint16_t remainLen = rwPos_;
      std::vector<uint8_t*> startLocs; // Pointers to start of each sequence found.

      while (remainLen >= seqSz - 1) {
        // Find sequence of the first ('seqSz' - 1) bytes of 'avoidSeq'
        seqStart = memmem(startSearch, remainLen, avoidSeq, seqSz - 1);
        if (seqStart == nullptr) {
          break;
        }

        // Update startSearch to start at 'seqSz' beyond the start of the found
        // sequence (i.e. right after where we intend to stuff the extra byte).
        startSearch = static_cast<uint8_t*>(seqStart) + (seqSz - 1);
        remainLen = static_cast<uint16_t>(data_ + rwPos_ - startSearch);

        startLocs.push_back(static_cast<uint8_t*>(seqStart));
      }

      if (startLocs.size() == 0) {
        return 0; // No need to stuff, "done" stuffing
      }

      // See if stuffing these bytes will exceed the underlying buffer size
      const uint8_t* const endOfBuf = rawBuf_ + rawBufSize_;
      if (data_ + rwPos_ + startLocs.size() >= endOfBuf) {
        return -1;
      }

      // Stuff extra bytes. We want to stuff bytes at an offset of (seqSz - 1)
      // from the beginning of each found sequence. For each start location of
      // a sequence, we need to shift the data after the target byte (where we
      // want to stuff) by 1. We must also consider that as we shift, we are
      // changing the start location of subsequent sequences.
      const uint8_t escByte = ~avoidSeq[seqSz - 1];
      for (uint16_t i = 0; i < startLocs.size(); i++) {
        // Shift data to create room to stuff a byte
        uint8_t* insertAt = startLocs[i] + i + (seqSz - 1);
        remainLen = static_cast<uint16_t>(data_ + rwPos_ - insertAt);
        memmove(insertAt + 1, insertAt, remainLen);

        // Stuff & increment rwPos_
        *insertAt = escByte;
        rwPos_++;
      }

      return static_cast<int16_t>(startLocs.size());
    }

    /**
     * @brief Inverse of byteStuff(). This method should only be called when
     * the frame's isValid() is true.
     *
     * Notes:
     *  1) This operation may shrink the data portion of the payload, thus
     *     invalidating the header's CRC and length fields.
     *
     * @param avoidSeq Pointer to a sequence of characters to be avoided.
     * @param seqSz Length of 'avoidSeq'. Must be at least 2.
     *
     * @return Returns the number of bytes de-stuffed/removed from the data.
     *         Returns -1 if any of the following are true:
     *          - 'avoidSeq' is NULL or 'seqSz' < 2
     *          - Current frame is not valid
     *          - A sequence of the first ('seqSz' - 1) bytes of 'avoidSeq'
     *            is found where the subsequent byte is not the inverse of the
     *            last byte of 'avoidSeq'.
     */
    int16_t byteDestuff(const uint8_t* avoidSeq, uint16_t seqSz) {
      // Sanity checks
      if (avoidSeq == nullptr || seqSz < 2) {
        return -1;
      } else if (this->isValid() == false) {
        return -1;
      }

      uint16_t dataLen = this->len();

      // Count the number of necessary extra bytes to remove. i.e. Count the
      // number of occurrences of the first ('seqSz' - 1) bytes of 'avoidSeq'.
      uint8_t* startSearch = data_;
      void* seqStart = nullptr;
      uint16_t remainLen = dataLen;
      std::vector<uint8_t*> startLocs; // Pointers to start of each sequence
      const uint8_t escByte = ~avoidSeq[seqSz - 1];

      while (remainLen >= seqSz) {
        seqStart = memmem(startSearch, remainLen, avoidSeq, seqSz - 1);
        if (seqStart == nullptr) {
          break;
        }

        // While we're at it, ensure that the "escape" byte follows the
        // sub-sequence found. If it is not there, then something went wrong
        // with the stuffing, or the frame is corrupted.
        if (static_cast<uint8_t*>(seqStart)[seqSz - 1] != escByte) {
          // Uh oh, something went wrong...
          return -1;
        }

        startSearch = static_cast<uint8_t*>(seqStart) + seqSz;
        remainLen = static_cast<uint16_t>(data_ + dataLen - startSearch);

        startLocs.push_back(static_cast<uint8_t*>(seqStart));
      }

      if (startLocs.size() == 0) {
        return 0; // No potential stuffing found, "done" de-stuffing
      }

      // Remove the "escape" bytes by shifting the data afterwards to fill
      // its place. Must also consider that as we shift, we are changing
      // the start locations of subsequent sequences.
      for (uint16_t i = 0; i < startLocs.size(); i++) {
        uint8_t* removeFrom = startLocs[i] - i + (seqSz - 1);

        // Shift data to fill space & decrement dataLen
        remainLen = static_cast<uint16_t>(data_ + dataLen - removeFrom);
        memmove(removeFrom, removeFrom + 1, remainLen);
        dataLen--;
      }

      return static_cast<int16_t>(startLocs.size());
    }

    /**
     * @brief Returns the the message ID field in the header.
     *
     * @return Returns the the message ID field in the header.
     */
    id_t id() const {
      return header_.id();
    }

    /**
     * @brief Returns the the length field in the header.
     *
     * @return Returns the the length field in the header.
     */
    len_t len() const {
      return header_.len();
    }

    /**
     * @brief Returns the CRC field in the header.
     *
     * @return Returns the CRC field in the header.
     */
    crc_t crc() const {
      return header_.crc();
    }

    /*
     * Assumes the header information is valid and checks the magic # and CRC.
     * May call writeHeader() beforehand to update the header information.
     *
     * Returns true if frame's magic number is as expected and the CRC check
     * is correct, otherwise returns false.
     */
    bool isValid() const {
      uint64_t frameSize = sizeof(MsgFrameHeader) + header_.len();
      if (frameSize > rawBufSize_ || frameSize > MAX_SIZE) {
        return false;
      } else if (rawBuf_ == nullptr) {
        return false;
      }

      // Set header CRC to 0 before calculating CRC, then restore
      MsgFrameHeader* pHead = (MsgFrameHeader*)rawBuf_;
      crc_t origCRC = pHead->crc();
      pHead->crc(0);
      crc_t calcCRC = 0;
      try {
        calcCRC = calcCRC_(rawBuf_, frameSize);
      } catch (std::exception& exc) {
        // TODO: Replace w/ log
        std::cerr << exc.what() << std::endl;
        pHead->crc(origCRC);
        return false;
      }
      pHead->crc(origCRC);

      return (MsgFrameHeader::MAGIC_NUMBER == header_.magic() &&
              calcCRC == header_.crc());
    }

    /**
     * @brief Returns the size of this Message Frame. As a pre-requisite, this
     * method requires the frame to be valid (i.e. isValid() is true).
     *
     * Internally, this is simply the header's length field + the header size.
     *
     * @return Returns the size of the Message Frame (header + data).
     *         Returns 0 if isValid() returns false.
     */
    uint16_t msgSize() const {
      if (this->isValid() == false) {
        return 0;
      }

      return (uint16_t)(sizeof(header_) + this->len());
    }

    /**
     * @brief Retrieves the size of this Message Frame (header + data
     * read/written so far). Note that if no data has been read yet, this
     * method just returns the size of the header.
     *
     * @return Size of header + data read/written so far.
     */
    uint16_t processedSize() const {
      return (uint16_t)(sizeof(header_) + rwPos_);
    }

    /**
     * @brief Writes a value to the data portion of the frame.
     *        Will simultaneously increment the read/write position.
     *
     * @tparam T Type of value to be written.
     * @param val Value to be written.
     *
     * @return Returns true if successful, false otherwise.
     *         Returning false may be due to a number of reasons:
     *          - data_ is NULL
     *          - Writing will exceed the bounds of the underlying buffer
     *          - The underlying output stream threw an exception
     */
    template <typename T>
    bool writeData(const T& val) {
      static_assert(std::is_trivially_copyable_v<T>,
          "Cannot write data that is not trivially copyable");

      if ((mode_ & MplexOpMode::WRITE) == 0) {
        return false;
      }

      if (data_ == nullptr || this->processedSize() + sizeof(T) > rawBufSize_) {
        return false;
      }

      try {
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
        // Reverse byte-order of basic integral types
        // If need be, also reverse order of floating types
        T tmpVal = val;
        if constexpr (std::is_integral_v<T>) {
          if constexpr (sizeof(T) == sizeof(uint8_t)) {
            // Do nothing
          } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
            tmpVal = htobe16(val);
          } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
            tmpVal = htobe32(val);
          } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            tmpVal = htobe64(val);
          } else {
            // TODO: Replace w/ log
            std::cerr << "ERROR: Integral type of uncommon length\n";
          }
        }
#if (__FLOAT_WORD_ORDER == __LITTLE_ENDIAN)
        else if constexpr (std::is_floating_point_v<T>) {
          if constexpr (sizeof(T) == sizeof(float)) {
            tmpVal = reverseFloat(val);
          } else if constexpr (sizeof(T) == sizeof(double)) {
            tmpVal = reverseDouble(val);
          } else {
            // TODO: Replace w/ log
            std::cerr << "ERROR: Floating type of uncommon length\n";
          }
        }
#endif
        outStream_.write<T>(tmpVal);

#elif (__BYTE_ORDER == __BIG_ENDIAN)
        outStream_.write<T>(val);
#else
#error  "Please fix <endian.h>"
#endif
      } catch (std::exception& exc) {
        // TODO: Replace w/ log
        std::cerr << "ERROR: Unable to write data to buffer\n";
        std::cerr << exc.what() << std::endl;
        return false;
      }

      // Increment inStream_'s read position & update rwPos_
      inStream_.skip(sizeof(T));
      rwPos_ = static_cast<uint16_t>(rwPos_ + sizeof(T));

      return true;
    }

    /**
     * @brief Reads a value from the data portion of the frame.
     *        Will simultaneously increment the read/write position.
     *
     * @tparam T Type of value to be read.
     * @param val Value to be read.
     *
     * @return Returns true if successful, false otherwise.
     *         Returning false may be due to a number of reasons:
     *          - data_ is NULL
     *          - Reading will exceed the bounds of the underlying buffer
     *          - The underlying input stream threw an exception
     */
    template <typename T>
    bool readData(T& val) {
      static_assert(std::is_trivially_copyable_v<T>,
          "Cannot read data that is not trivially copyable");

      if ((mode_ & MplexOpMode::READ) == 0) {
        return false;
      }

      if (data_ == nullptr || this->processedSize() + sizeof(T) > rawBufSize_) {
        return false;
      }

      try {
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
        // Reverse byte-order of basic integral types
        // If need be, also reverse order of floating types
        inStream_.read(val);
        T tmpVal = val;

        if constexpr (std::is_integral_v<T>) {
          if constexpr (sizeof(T) == sizeof(uint8_t)) {
            val = tmpVal;
          } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
            val = be16toh(tmpVal);
          } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
            val = be32toh(tmpVal);
          } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            val = be64toh(tmpVal);
          } else {
            // TODO: Replace w/ log
            std::cerr << "ERROR: Integral type of uncommon length\n";
          }
        }
#if (__FLOAT_WORD_ORDER == __LITTLE_ENDIAN)
        else if constexpr (std::is_floating_point_v<T>) {
          if constexpr (sizeof(T) == sizeof(float)) {
            val = reverseFloat(tmpVal);
          } else if constexpr (sizeof(T) == sizeof(double)) {
            val = reverseDouble(tmpVal);
          } else {
            // TODO: Replace w/ log
            std::cerr << "ERROR: Floating type of uncommon length\n";
          }
        }
#endif

#elif (__BYTE_ORDER == __BIG_ENDIAN)
        inStream_.read(val);
#else
#error  "Please fix <endian.h>"
#endif
      } catch (std::exception& exc) {
        // TODO: Replace w/ log
        std::cerr << "ERROR: Unable to read data from buffer\n";
        std::cerr << exc.what() << std::endl;
        return false;
      }

      // Increment outStream_'s read position & update rwPos_
      outStream_.skip(sizeof(T));
      rwPos_ = static_cast<uint16_t>(rwPos_ + sizeof(T));

      return true;
    }

    // Returns:
    //  - Pointer to the start of the data section of the MsgFrame; or
    //  - NULL if this frame is an End of Message Group frame.
    uint8_t* getData() const {
      return this->isEndOfMsgGroup() ? nullptr : data_;
    }

    const uint8_t* getBuf() {
      return rawBuf_;
    }

#ifdef DEBUG
    void printFrame() const {
      uint8_t cnt = 0;
      uint16_t procSize = this->processedSize();
      for (const uint8_t* i = rawBuf_; i < rawBuf_ + procSize; i++) {
        printf("%02X ", *i);

        if (++cnt == 16) {
          cnt = 0;
          printf("\n");
        }
      }
      printf("\n\n");
    }
#endif
};

#endif
