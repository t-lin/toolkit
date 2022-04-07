#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <tins/small_uint.h>

// Use https://users.ece.cmu.edu/~koopman/crc/crc32.html to help find appropriate CRCs
// given the expected data length and desired Hamming Distance (HD).

// 0x12F = 0x97 w/ explicit +1
#define CRC_POLY_8 0x12F

// 0x6E57 = 0x372B w/ explicit +1
#define CRC_POLY_14 0x6E57

// 0x104C11DB7 = 0x82608EDB w/ explicit +1
#define CRC_POLY_32 0x104C11DB7

// Table for looking up CRC polynomials based on bit length
const uint64_t CRC_POLY_TABLE[] = {
    0, 0, 0, 0, 0, 0, 0, 0, CRC_POLY_8, 0, 0,
    0, 0, 0, CRC_POLY_14, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, CRC_POLY_32
};

using Tins::small_uint;

template <uint32_t N>
typename small_uint<N>::repr_type crc(uint8_t* data, size_t dataLen, small_uint<N> init) {
  const small_uint<N> MSB_ONE = 1 << (N -1);
  const uint64_t CRC_POLY = CRC_POLY_TABLE[N];
  const auto MAX_VAL = Tins::small_uint<N>::max_value;

  typename small_uint<N>::repr_type crc = init;
  for (size_t i = 0; i < dataLen; i++) {
    crc = (crc ^ (data[i] << (N - 8))) & MAX_VAL;
    for (int bit = 0; bit < 8; bit++) {
      if (0 == (crc & MSB_ONE)) {
        crc = (crc << 1) & MAX_VAL;
      } else {
        crc = ((crc << 1) ^ CRC_POLY) & MAX_VAL;
      }
    }
  }

  return crc;
}

// Use these pages to help double-check the results:
//  - http://www.ghsi.de/pages/subpages/Online%20CRC%20Calculation/indexDetails.php
//  - https://string-functions.com/string-hex.aspx
int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "ERROR: Expecting one parameter (string to calculate checksum over)\n");
    return 1;
  }

  uint8_t* data = (uint8_t*)argv[1];
  size_t dataSize = strlen((char*)data);

  // For debugging
  //printf("Size of data is: %lu\n", dataSize);
  //for (size_t i = 0; i < dataSize; i++) {
  //  printf("%02X ", data[i]);
  //}
  //printf("\n");

  small_uint<8> result8 = 0;
  small_uint<14> result14 = 0;
  small_uint<32> result32 = 0;

  result8 =  crc<8>(data, dataSize, 0x0);
  result14 =  crc<14>(data, dataSize, 0x0);
  result32 =  crc<32>(data, dataSize, 0x0);

  printf("CRC-8 of data is: 0x%02X\n", (small_uint<8>::repr_type)result8);
  printf("CRC-14 of data is: 0x%04X\n", (small_uint<14>::repr_type)result14);
  printf("CRC-32 of data is: 0x%08X\n", (small_uint<32>::repr_type)result32);

  return 0;
}
