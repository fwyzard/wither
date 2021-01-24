#include <iomanip>
#include <iostream>

#include "invert.h"

template <typename T>
std::string binary(T value, size_t bits = sizeof(T) * 8) {
  std::string out(bits + 2, '\0');
  out[0] = '0';
  out[1] = 'b';
  for (size_t i = 0; i < bits; ++i) {
    out[i + 2] = (value & (0x01 << (bits - i - 1)) ? '1' : '0');
  }
  return out;
}

template <typename T>
void log(T value, T inverted, T expected, unsigned int bits = sizeof(T) * 8) {
  std::cout << "value:    " << binary(value, bits) << std::endl;
  std::cout << "inverted: " << binary(inverted, bits) << std::endl;
  std::cout << "expected: " << binary(expected, bits) << std::endl;
  std::cout << std::endl;
}

int main(int argc, const char* argv[]) {
  {
    // invert one byte - 0x42 is palindrome with respect to bit inversion
    uint8_t value = 0x42;     // 0b01000010
    uint8_t expected = 0x42;  // 0b01000010
    uint8_t inverted = invert_bits(value);
    log(value, inverted, expected);
    assert(inverted == expected);
  }

  {
    // invert the 2 lowest bits
    uint8_t value = 0x42;     // 0b......10
    uint8_t expected = 0x01;  // 0b......01
    uint8_t inverted = invert_bits(value, 2);
    log(value, inverted, expected, 2);
    assert(inverted == expected);
  }

  {
    // invert a 16-bit word
    uint16_t value = 0xBEEF;     // 0b1011111011101111
    uint16_t expected = 0xF77D;  // 0b1111011101111101
    uint16_t inverted = invert_bits(value);
    log(value, inverted, expected);
    assert(inverted == expected);
  }

  {
    // invert 24 bits out of a 32-bit word
    uint32_t value = 0xC0FFEE;     // 0b110000001111111111101110
    uint32_t expected = 0x77FF03;  // 0b011101111111111100000011
    uint32_t inverted = invert_bits(value, 24);
    log(value, inverted, expected, 24);
    assert(inverted == expected);
  }
}
