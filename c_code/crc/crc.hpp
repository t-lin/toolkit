#pragma once
#ifndef UTILS_CRC_H
#define UTILS_CRC_H

// C libs
#include <stdint.h>

// C++ libs
#include <array>

// Tins lib
#include <tins/small_uint.h>

// Use https://users.ece.cmu.edu/~koopman/crc/crc32.html to help find appropriate CRCs
// given the expected data length and desired Hamming Distance (HD).

namespace CRCUtils {

#define NO_POLY (0X0)

// NOTE: The polynomials selected below attempt to maintain decent error
//       detection up to a Hamming Distance of 4 at higher data lengths,
//       even if it increases the likelihood of false negatives.

// 0xB = 0x5 w/ explicit +1
#define CRC_POLY_3 (0xB)

// 0x13 = 0x9 w/ explicit +1
#define CRC_POLY_4 (0x13)

// 0x3D = 0x1E w/ explicit +1
#define CRC_POLY_5 (0x3D)

// 0x67 = 0x33 w/ explicit +1
#define CRC_POLY_6 (0x67)

// 0xCB = 0x65 w/ explicit +1
#define CRC_POLY_7 (0xCB)

// 0x12F = 0x97 w/ explicit +1
#define CRC_POLY_8 (0x12F)

// 0x2FB = 0x17D w/ explicit +1
#define CRC_POLY_9 (0x2FB)

// 0x48F = 0x247 w/ explicit +1
#define CRC_POLY_10 (0x48F)

// 0xB07 = 0x583 w/ explicit +1
#define CRC_POLY_11 (0xB07)

// 0x11E7 = 0x8F3 w/ explicit +1
#define CRC_POLY_12 (0x11E7)

// 0x25CD = 0x12E6 w/ explicit +1
#define CRC_POLY_13 (0x25CD)

// 0x6E57 = 0x372B w/ explicit +1
// HD=4 up to 8176
#define CRC_POLY_14 (0x6E57)

// 0x860D = 0x4306 w/ explicit +1
#define CRC_POLY_15 (0x860D)

// 0x1A2EB = 0xD175 w/ explicit +1
#define CRC_POLY_16 (0x1A2EB)

// 0x22CBB = 0x1165D w/ explicit +1
#define CRC_POLY_17 (0x22CBB)

// 0x40031 = 0x20018 w/ explicit +1
#define CRC_POLY_18 (0x40031)

// 0xEF61F = 0x77B0F w/ explicit +1
#define CRC_POLY_19 (0xEF61F)

// 0x18359F = 0xC1ACF w/ explicit +1
#define CRC_POLY_20 (0x18359F)

// 0x21BF1F = 0x10DF8F w/ explicit +1
#define CRC_POLY_21 (0x21BF1F)

// 0x490F29 = 0x248794 w/ explicit +1
#define CRC_POLY_22 (0x490F29)

// 0x8002A9 = 0x400154 w/ explicit +1
#define CRC_POLY_23 (0x8002A9)

// 0x1328B63 = 0x9945B1 w/ explicit +1
#define CRC_POLY_24 (0x1328B63)

// 0x217745B = 0x10BBA2D w/ explicit +1
#define CRC_POLY_25 (0x217745B)

// 0x4F1692B = 0x278B495 w/ explicit +1
#define CRC_POLY_26 (0x4F1692B)

// 0xE8BD17F = 0x745E8BF w/ explicit +1
#define CRC_POLY_27 (0xE8BD17F)

// 0x16CF6A23 = 0xB67B511 w/ explicit +1
#define CRC_POLY_28 (0x16CF6A23)

// 0x200000BF = 0x1000005F w/ explicit +1
#define CRC_POLY_29 (0x200000BF)

// 0x44A8653B = 0x2254329D w/ explicit +1
#define CRC_POLY_30 (0x44A8653B)

// 0xA5548665 = 0x52AA4332 w/ explicit +1
#define CRC_POLY_31 (0xA5548665)

// 0x104C11DB7 = 0x82608EDB w/ explicit +1
#define CRC_POLY_32 (0x104C11DB7)

// Table for looking up CRC polynomials based on bit length
// TODO: Add more polynomials to support up to 64-bit CRCs
constexpr uint64_t CRC_POLY_TABLE[] = {
    NO_POLY, NO_POLY, NO_POLY, CRC_POLY_3, CRC_POLY_4,
    CRC_POLY_5, CRC_POLY_6, CRC_POLY_7, CRC_POLY_8, CRC_POLY_9,
    CRC_POLY_10, CRC_POLY_11, CRC_POLY_12, CRC_POLY_13, CRC_POLY_14,
    CRC_POLY_15, CRC_POLY_16, CRC_POLY_17, CRC_POLY_18, CRC_POLY_19,
    CRC_POLY_20, CRC_POLY_21, CRC_POLY_22, CRC_POLY_23, CRC_POLY_24,
    CRC_POLY_25, CRC_POLY_26, CRC_POLY_27, CRC_POLY_28, CRC_POLY_29,
    CRC_POLY_30, CRC_POLY_31, CRC_POLY_32
};

#define CRC_INIT (0xDEADBEEFFEEDFACE)

#define BYTE_NUM_VALS (256UL)

/**
 * @brief Pre-generate look-up table to help speed up CRC calculations.
 *
 * @tparam N The CRC bit-width.
 * @tparam crcPoly The generator polynomial to use for calculating the CRC.
 *
 * @return Array of 256 unsigned integers, each one at least N-bits in width
 */
template <uint32_t N>
constexpr std::array<typename Tins::small_uint<N>::repr_type, BYTE_NUM_VALS>
generateCRCLUT(const uint64_t crcPoly) {
  static_assert(N >= 8); // TODO: Modify later to support N < 8

  using repr_type = typename Tins::small_uint<N>::repr_type;
  constexpr repr_type MSB_ONE = 1UL << (N - 1);
  constexpr repr_type MAX_VAL = Tins::small_uint<N>::max_value;
  std::array<repr_type, BYTE_NUM_VALS> table = {};

  for (uint32_t i = 0; i < BYTE_NUM_VALS; i++) {
    uint64_t crc = 0;

    crc = crc ^ (i << (N - 8));
    for (int bit = 0; bit < 8; bit++) {
      if (0 == (crc & MSB_ONE)) {
        crc = crc << 1;
      } else {
        crc = (crc << 1) ^ crcPoly;
      }
    }

    table[i] = crc & MAX_VAL;
  }

  return table;
}

/**
 * @brief CRC calculation function for custom-width CRCs.
 *        Credit for look-up table optimization of CRC:
 *          - https://create.stephan-brumme.com/crc32/
 *
 * @tparam N The CRC bit-width.
 * @tparam crcPoly The generator polynomial to use for calculating the CRC.
 *
 * @param data Pointer to data to calculate CRC for.
 * @param dataLen Length of data.
 * @param init Initial value for CRC calculation.
 *
 * @return The calculated CRC of the data, in the closest unsigned integer type
 *         given the specified CRC bit-width 'N'.
 */
template <uint32_t N, uint64_t crcPoly>
typename Tins::small_uint<N>::repr_type CRC(const uint8_t* const data,
                                            uint64_t dataLen,
                                            Tins::small_uint<N> init) {
  static_assert(N >= 8); // TODO: Modify later to support N < 8
  static_assert(N < sizeof(CRC_POLY_TABLE));
  static_assert(crcPoly != 0);

  using repr_type = typename Tins::small_uint<N>::repr_type;
  constexpr repr_type MAX_VAL = Tins::small_uint<N>::max_value;
  static constexpr auto LUT = generateCRCLUT<N>(crcPoly);

  if (data == nullptr) {
    throw std::invalid_argument("Cannot calculate CRC w/ NULL buffer");
  }

  uint64_t crc = init;
  for (uint64_t i = 0; i < dataLen; i++) {
    crc = (LUT[(crc >> (N - 8)) ^ data[i]] ^ (crc << 8)) & MAX_VAL;
  }

  return crc & MAX_VAL;
}

/**
 * @brief CRC calculation function for custom-width CRCs.
 *        Will automatically select a generator polynomial depending on the
 *        CRC bit-width 'N'.
 *
 * @tparam N The CRC bit-width.
 *
 * @param data Pointer to data to calculate CRC for.
 * @param dataLen Length of data.
 * @param init Initial value for CRC calculation.
 *
 * @return The calculated CRC of the data, in the closest unsigned integer type
 *         given the specified CRC bit-width 'N'.
 */
template <uint32_t N>
typename Tins::small_uint<N>::repr_type CRC(const uint8_t* const data,
                                            uint64_t dataLen,
                                            Tins::small_uint<N> init) {
  static_assert(N >= 8); // TODO: Modify later to support N < 8
  static_assert(N < sizeof(CRC_POLY_TABLE));
  static_assert(CRC_POLY_TABLE[N] != 0);

  return CRC<N, CRC_POLY_TABLE[N]>(data, dataLen, init);
}

} // CRCUtils namespace

#endif
