#pragma once
#ifndef MPLEX_MSG_FRAME_HEADERS_H
#define MPLEX_MSG_FRAME_HEADERS_H

// C headers
#include <asm/byteorder.h>
#include <stdint.h>

// libtins
#include <tins/small_uint.h>

#include "../../crc/crc.hpp"

typedef uint8_t vers_t;

// NOTE: For now, only supporting compilers & archs that support packed
//       objects. This is enforced via static_asserts to check the size of
//       structures. This assumption enables us to leverage memcpy and
//       memcmp for de/serialization and comparisons.

/*
 * Bit layout of v0 multiplex message frame header, in big-endian byte order.
 *  - NOTE: The drawing below is not to scale.
 *
 * The header consists of 5 Bytes, which are broken down into their various
 * header fields as follows:
 *  0                8                16               24
 * +----------------+----------------+----------------+----------------+
 * |     Magic #    |    Msg ID    |    Msg Data Length    |Msg CRC... |
 * |<--- 8 Bits --->|<-- 7 Bits -->|<------ 11 Bits ------>|<--------- |
 * +----------------+----------------+----------------+----------------+
 *  32
 * +----------------+
 * | ... Msg CRC    |
 * | 14 Bits ------>|
 * +----------------+
 */
class __attribute__((packed)) MsgFrameHeader_v0 {
  public:
    static const vers_t VERS = 0;
    static const uint8_t MAGIC_NUMBER = 0x7E;

    static const uint8_t ID_WIDTH = 7;   // Bit-width of ID field
    static const uint8_t LEN_WIDTH = 11; // Bit-width of length field
    static const uint8_t CRC_WIDTH = 14; // Bit-width of CRC field

    static const uint64_t CRC_POLY = CRCUtils::CRC_POLY_TABLE[CRC_WIDTH];
    static_assert(CRC_POLY != 0);

    // Header-dependent types
    typedef Tins::small_uint<ID_WIDTH> id_t;
    typedef Tins::small_uint<LEN_WIDTH> len_t;
    typedef Tins::small_uint<CRC_WIDTH> crc_t;

    // End of Message Group ID
    static const id_t::repr_type END_OF_GROUP_ID = id_t::max_value;

  private:
    // Magic number
    uint8_t magic_ = MsgFrameHeader_v0::MAGIC_NUMBER;

#if defined (__BIG_ENDIAN_BITFIELD)
    uint32_t id_:ID_WIDTH,    // Message type/topic ID
             len_:LEN_WIDTH,  // Length of message data in Bytes
             crc_:CRC_WIDTH;  // Checksum for whole frame (i.e. header + data)
#elif defined (__LITTLE_ENDIAN_BITFIELD)
    /*
     * Not trivial to use bit-fields on little-endian machines when the
     * base type is a multi-byte integral type. For Bytes [1, 4] (indexed from
     * 0) of the header (i.e. bits [8, 39]), we use four Bytes and perform
     * the appropriate bit manipulation in setters/getters.
     *
     * These are pseudo-named fields, which are not bit-width accurate, and are
     * named after whichever field holds the most significant bits in that Byte.
     *  8                16                24               32
     * +----------------+----------------+----------------+----------------+
     * |    "Msg ID"    |   "Msg Data    |   "Msg Data    |   "Msg CRC"    |
     * |                |   Length"[0]   |   Length"[1]   |                |
     * +----------------+----------------+----------------+----------------+
     * |<-- 7 Bits -->|<------ 11 Bits ------>|<--------- 14 Bits -------->|
     */
    uint8_t id_ = END_OF_GROUP_ID << 1;
    uint8_t len_[2] = {0};
    uint8_t crc_ = 0;
#else
#error  "Please fix <asm/byteorder.h>"
#endif

  public:
    MsgFrameHeader_v0() {
#if defined (__BIG_ENDIAN_BITFIELD)
      // Bit-fields currently do not support default initialization
      // Initialize to End of Message Group values
      id_ = END_OF_GROUP_ID;
      len_ = 0;
      crc_ = 0;
#endif
    }

    void setMagic() {
      magic_ = MsgFrameHeader_v0::MAGIC_NUMBER;
    }

    uint8_t magic() const {
      return magic_;
    }

#if defined (__BIG_ENDIAN_BITFIELD)
    void id(id_t id) {
      id_ = id;
    }

    id_t id() const {
      return id_;
    }

    void len(len_t len) {
      len_ = len;
    }

    len_t len() const {
      return len_;
    }

    void crc(crc_t crc) {
      crc_ = crc;
    }

    crc_t crc() const {
      return crc_;
    }
#elif defined (__LITTLE_ENDIAN_BITFIELD)
    /*
     * For reference:
     *  8                16                24               32
     * +----------------+----------------+----------------+----------------+
     * |    "Msg ID"    |   "Msg Data    |   "Msg Data    |   "Msg CRC"    |
     * |                |   Length"[0]   |   Length"[1]   |                |
     * +----------------+----------------+----------------+----------------+
     * |<-- 7 Bits -->|<------ 11 Bits ------>|<--------- 14 Bits -------->|
     */
    void id(id_t id) {
      id_ = static_cast<uint8_t>(id << 1) | (id_ & 0x1);
    }

    id_t id() const {
      return static_cast<id_t::repr_type>(id_ >> 1);
    }

    void len(len_t len) {
      id_ = (id_ & 0xFE) | static_cast<uint8_t>(len >> 10);
      len_[0] = static_cast<uint8_t>(len >> 2);
      len_[1] = static_cast<uint8_t>(len << 6) | (len_[1] & 0x3F);
    }

    len_t len() const {
      return ((id_ << 10) |
             (len_[0] << 2) |
             (len_[1] >> 6)) & len_t::max_value;
    }

    void crc(crc_t crc) {
      len_[1] = (len_[1] & 0xC0) | static_cast<uint8_t>(crc >> 8);
      crc_ = static_cast<uint8_t>(crc & 0xFF);
    }

    crc_t crc() const {
      return ((len_[1] << 8) | crc_) & crc_t::max_value;
    }
#else
#error  "Please fix <asm/byteorder.h>"
#endif
};
static_assert(sizeof(MsgFrameHeader_v0) == 5,
              "Size of MsgFrameHeader_v0 != 5");

#endif

