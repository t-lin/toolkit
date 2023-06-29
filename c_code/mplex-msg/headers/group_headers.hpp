#pragma once
#ifndef MPLEX_MSG_GROUP_HEADERS_H
#define MPLEX_MSG_GROUP_HEADERS_H

// C headers
#include <asm/byteorder.h>

#include "../../crc/crc.hpp"

typedef uint8_t vers_t;

// NOTE: For now, only supporting compilers & archs that support packed
//       objects. This is enforced via static_asserts to check the size of
//       structures. This assumption enables us to leverage memcpy and
//       memcmp for de/serialization and comparisons.

/*
 * Bit layout of v0 multiplex message group header, in big-endian byte order.
 *  - NOTE: The drawing below is not to scale.
 *
 * The header consists of 13 Bytes, which are broken down into their various
 * header fields as follows:
 *  0                8                16               24
 * +----------------+----------------+----------------+----------------+
 * |     Magic #    |    Version #   |             Timestamp (sec) ... |
 * |<--- 8 Bits --->|<--- 8 Bits --->|<-------------------- 32 Bits -- |
 * +----------------+----------------+----------------+----------------+
 *  32               40               48               56
 * +----------------+----------------+----------------+----------------+
 * | ... Timestamp (sec) cont.       |            Timestamp (nsec) ... |
 * | ------------------------------->|<-------------------- 32 Bits -- |
 * +----------------+----------------+----------------+----------------+
 *  64               72               80               88
 * +----------------+----------------+----------------+----------------+
 * | ... Timestamp (nsec) cont.      |     # Msg Frames    |Header Len.|
 * | ------------------------------->|<----- 11 Bits ----->|<-5 Bits-->|
 * +----------------+----------------+----------------+----------------+
 *  96
 * +----------------+
 * |Header-only CRC |
 * |<--- 8 Bits --->|
 * +----------------+
 */
class __attribute__((packed)) MsgGroupHeader_v0 {
  public:
    static const vers_t VERS = 0;
    static const uint8_t MAGIC_NUMBER = 0xAA;

    static const uint8_t NUM_FRAMES_WIDTH = 11; // Bit-width of numFrames_
    static const uint8_t HLEN_WIDTH = 5;        // Bit-width of headerLen_
    static const uint8_t HCRC_WIDTH = 8;        // Bit-width of header CRC

    static const uint16_t CRC_POLY = CRCUtils::CRC_POLY_TABLE[HCRC_WIDTH];
    static_assert(CRC_POLY != 0);

    // Header-dependent types
    typedef Tins::small_uint<NUM_FRAMES_WIDTH> nFrames_t;
    typedef Tins::small_uint<HLEN_WIDTH> len_t;
    typedef Tins::small_uint<HCRC_WIDTH> hcrc_t;

  private:
    // Magic number
    uint8_t magic_ = MsgGroupHeader_v0::MAGIC_NUMBER;

    // Version of header & Message Frames
    const vers_t vers_ = MsgGroupHeader_v0::VERS;

    // Timestamp (32-bit sec & 32-bit nanosec)
    uint32_t timestamp_s_ = 0;
    uint32_t timestamp_ns_ = 0;

#if defined (__BIG_ENDIAN_BITFIELD)
    uint16_t numFrames_:NUM_FRAMES_WIDTH,  // Number of message frames
             headerLen_:LEN_WIDTH;         // Header length in Bytes
#elif defined (__LITTLE_ENDIAN_BITFIELD)
    /*
     * Not trivial to use bit-fields on little-endian machines when the
     * base type is a multi-byte integral type. For Bytes 10 and 11 (indexed
     * from 0) of the header (i.e. bits [80, 95]), we use two Bytes and
     * perform the appropriate bit manipulation in setters/getters.
     *
     * These are pseudo-named fields, which are not bit-width accurate, and are
     * named after whichever field holds the most significant bits in that Byte,
     * unless another field terminates in the least significant bit.
     *  80               88
     * +----------------+----------------+
     * | "# Msg Frames" | "Header Len."  |
     * +----------------+----------------+
     * |<----- 11 Bits ----->|<-5 Bits-->|
     */
    uint8_t numFrames_ = 0;
    uint8_t headerLen_ = 0;
#else
#error  "Please fix <asm/byteorder.h>"
#endif

    // Checksum of header
    hcrc_t::repr_type hcrc_ = 0;

  public:
    void setMagic() {
      magic_ = MsgGroupHeader_v0::MAGIC_NUMBER;
    }

    uint8_t magic() const {
      return magic_;
    }

    vers_t vers() const {
      return vers_;
    }

#if defined (__BIG_ENDIAN_BITFIELD)
    void timestamp(uint32_t sec, uint32_t nsec) {
      timestamp_s_ = sec;
      timestamp_ns_ = nsec;
    }

    uint64_t timestamp() const {
      return (((uint64_t)timestamp_s_) << 32) | timestamp_ns_;
    }

    void numFrames(nFrames_t nFrames) {
      numFrames_ = nFrames;
    }

    nFrames_t numFrames() const {
      return numFrames_;
    }

    void headerLen(len_t len) {
      headerLen_ = len;
    }

    len_t headerLen() const {
      return headerLen_;
    }
#elif defined (__LITTLE_ENDIAN_BITFIELD)
    void timestamp(uint32_t sec, uint32_t nsec) {
      timestamp_s_ = htobe32(sec);
      timestamp_ns_ = htobe32(nsec);
    }

    uint64_t timestamp() const {
      return (((uint64_t)be32toh(timestamp_s_)) << 32) | be32toh(timestamp_ns_);
    }

    /*
     * For reference:
     *  80               88
     * +----------------+----------------+
     * | "# Msg Frames" | "Header Len."  |
     * +----------------+----------------+
     * |<----- 11 Bits ----->|<-5 Bits-->|
     */
    void numFrames(nFrames_t nFrames) {
      numFrames_ = static_cast<uint8_t>(nFrames >> 3);
      headerLen_ = static_cast<uint8_t>(nFrames << 5) | (headerLen_ & 0x1F);
    }

    nFrames_t numFrames() const {
      return ((numFrames_ << 3) | (headerLen_ >> 5)) & nFrames_t::max_value;
    }

    void headerLen(len_t len) {
      headerLen_ = (headerLen_ & 0xE0) | len;
    }

    len_t headerLen() const {
      return headerLen_ & len_t::max_value;
    }
#else
#error  "Please fix <asm/byteorder.h>"
#endif

    void hcrc(hcrc_t hcrc) {
      hcrc_ = hcrc;
    }

    hcrc_t hcrc() const {
      return hcrc_;
    }
};
static_assert(sizeof(MsgGroupHeader_v0) == 13);

#endif
