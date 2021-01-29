#include <iomanip>
#include <iostream>
#include <string>

#include "bitstream.h"
#include "huffman.h"

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

int main(int argc, const char* argv[]) {

  const std::string message = "hello world!";

  bitstream stream;

  {
    std::cout << "original message: \"" << message << '"' << std::endl;
    huffman_encoding huffman((uint8_t const*) message.data(), message.size());
    huffman.serialise(stream);
    size_t header_size = stream.size();
    std::cout << std::endl;
    std::cout << "Huffman header (" << header_size << " bits)" << std::endl;
    stream.skip(header_size);

    for (uint8_t byte: message) {
      huffman.encode(stream, byte);
      uint64_t value;
      size_t read = stream.read(64, value);
      std::cout << "byte '" << byte << "' encoded as (" << read << " bits) " << binary(value, read) << std::endl;
    }

    stream.seekg(header_size);
    uint64_t value;
    size_t read = stream.read(64, value);
    std::cout << "encoded message: (" << read << " bits) " << binary(value, read) << std::endl;
    std::cout << std::endl;
    stream.seekg(0);
  }

  std::string decoded;

  {
    huffman_encoding huffman;
    huffman.deserialise(stream);
    size_t header_size = stream.tellg();
    std::cout << "Huffman header (" << header_size << " bits)" << std::endl;

    while (true) {
      uint8_t byte;
      size_t pos = stream.tellg();
      std::cout << "at " << pos - header_size << "/" << stream.size() - header_size << ": ";
      if (not huffman.decode(stream, byte)) {
        std::cout << "done" << std::endl;
        break;
      }
      size_t read = stream.tellg() - pos;
      stream.seekg(pos);
      uint64_t value;
      stream.read(read, value);
      std::cout << "byte '" << byte << "' (" << read << " bits) " << binary(value, read) << std::endl;
      decoded.append(1, (char) byte);
    }
    std::cout << std::endl;
    std::cout << "decoded message:  \"" << decoded << '"' << std::endl;
  }

  assert(message == decoded);
}
