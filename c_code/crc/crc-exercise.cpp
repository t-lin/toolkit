#include <string.h>
#include <unordered_map>

#include <stdio.h>
#include <stdint.h>

#include <tins/small_uint.h>

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
using std::unordered_map;

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

int main()
{
  const uint32_t wordLen = 16;
  const uint32_t crcLen = 8;
  const uint64_t MAX_WORD_VAL = small_uint<wordLen>::max_value;
  const uint64_t MAX_CODEWORD_VAL = small_uint<wordLen + crcLen>::max_value;
  //printf("Max word val is: %lu\n", MAX_WORD_VAL);

  // Map CRC results to a counter
  // Counts how many words match the given CRC
  unordered_map<small_uint<wordLen + crcLen>::repr_type, uint64_t> crcMap;

  small_uint<crcLen> crcRes = 0, newCrc = 0;
  small_uint<wordLen> word = 0;
  small_uint<wordLen + crcLen> codeWord = 0;
  for (word = 0; word < MAX_WORD_VAL; word = word + 1) {
    if (word % 1000 == 0) {
      printf("word = %hu\n", (small_uint<wordLen>::repr_type)word);
    }

    crcRes = crc<crcLen>((uint8_t*)&word, sizeof(word), 0x0);
    codeWord = (word << crcLen) | crcRes;

    // "Induce bitflips" in codeword, then re-calculate the CRC on the word
    // portion of the corrupted result. If they match, then the CRC has failed.
    // The HD can be calculated by simple XOR and checking the # of 1s.
    //
    // We "induce bitflips" by exhaustively enumerating all possible values of
    // the codeword's length.
    small_uint<wordLen + crcLen> corruptedCodeWord = 0;
    for (corruptedCodeWord = 0;
          corruptedCodeWord < MAX_CODEWORD_VAL;
          corruptedCodeWord = corruptedCodeWord + 1) {
      // TODO: Finish this
    }

    crcMap[codeWord]++;
  }

  // Calculate one more time for actual max val
  crcRes = crc<crcLen>((uint8_t*)&word, sizeof(word), 0x0);
  crcMap[crcRes]++;

  for (auto& pair : crcMap) {
    if (pair.second > 1) {
      printf("CRC %u: %lu instances\n", pair.first, pair.second);
    }
  }

  return 0;
}
