#include <iomanip>
#include <iostream>

#include "bitstream.h"

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
void log(T value, T expected, unsigned int bits = sizeof(T) * 8) {
  std::cout << "value:    " << binary(value, bits) << std::endl;
  std::cout << "expected: " << binary(expected, bits) << std::endl;
  std::cout << std::endl;
}

int main(int argc, const char* argv[]) {
  bitstream stream;

  {
    // write "42" one bit at a time, starting from the LSB
    stream.write(1, 0);
    stream.write(1, 1);
    stream.write(1, 0);
    stream.write(1, 1);
    stream.write(1, 0);
    stream.write(1, 1);
    stream.write(1, 0);
    stream.write(1, 0);
  }

  {
    // read six bits and check that the result is "42"
    uint8_t expected = 0b101010;
    uint8_t value;
    size_t read = stream.read(6, value);
    log(value, expected, read);
    assert(read == 6 and value == expected);
  }

  {
    // write 60 bits out of a 64-bit word
    uint64_t value = 0b1100'00001010'00011110'01011100'11101101'11001010'10110001'11100101;
    stream.write(60, value);
  }

  {
    // read 16 bits at a time (including 2 bits left over from the previous read)
    uint16_t expected = 0b11000111'10010100;
    uint16_t value;
    size_t read = stream.read(16, value);
    log(value, expected, read);
    assert(read == 16 and value == expected);

    expected = 0b10110111'00101010;
    read = stream.read(16, value);
    log(value, expected, read);
    assert(read == 16 and value == expected);

    expected = 0b01111001'01110011;
    read = stream.read(16, value);
    log(value, expected, read);
    assert(read == 16 and value == expected);

    expected = 0b110000'00101000;
    read = stream.read(16, value);
    log(value, expected, read);
    assert(read == 14 and value == expected);
  }

}
