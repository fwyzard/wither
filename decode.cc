// decode an arbitrary input from 1-byte Huffman coding and output the result

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <fmt/printf.h>

#include "bitstream.h"
#include "huffman.h"


int main(int argc, const char* argv[]) {
  // no need to synchronise the C++ I/O streams with the C I/O streams
  std::ios::sync_with_stdio(false);

  // stream for the input data
  std::istream* in;
  if (argc <= 1 or strcmp(argv[1], "-") == 0) {
    // reopen cin/stdin in binary mode
    assert(freopen(nullptr, "rb", stdin));
    in = &std::cin;
  } else {
    // open the file given on the commmand line argument
    in = new std::ifstream(argv[1], std::ios::in | std::ios::binary);
  }

  // stream for the output data
  std::ostream* out;
  if (argc <= 2 or strcmp(argv[2], "-") == 0) {
    // reopen cout/stdout in binary mode
    assert(freopen(nullptr, "rb", stdout));
    out = &std::cout;
  } else {
    out = new std::ofstream(argv[2], std::ios::out | std::ios::binary);
  }

  // buffer the input data
  std::vector<uint8_t> input_buffer(std::istreambuf_iterator<char>(*in), {});

  // close the input stream
  if (in != &std::cin) {
    delete in;
  }

  // copy the input buffer to a bitstream
  bitstream decoding_buffer;
  decoding_buffer.from_bytes(input_buffer);

  // deserialise the canonical Huffman coding from the input
  huffman_encoding encoding;
  encoding.deserialise(decoding_buffer);

  // cut the bitstream to the size of the encoded message
  assert(decoding_buffer.size() >= encoding.header_size_ + encoding.encoded_size_);
  decoding_buffer.resize(encoding.header_size_ + encoding.encoded_size_);

  // decode the input according to the Huffman coding
  std::vector<uint8_t> output_buffer;
  output_buffer.reserve(encoding.original_size_);
  uint8_t byte;
  while (encoding.decode(decoding_buffer, byte)) {
    output_buffer.push_back(byte);
  }

  out->write((const char*) output_buffer.data(), output_buffer.size());
  out->flush();
  /*
  std::cerr << "input buffer size:  " << input_buffer.size() << " " << huffman_encoding::alphabet_bits << "-bit characters" << std::endl;
  std::cerr << "output buffer size: " << (encoding.header_size_ + encoding.encoded_size_ + 7) / 8 << " bytes" << std::endl;
  std::cerr << "output buffer size: " << (encoding_buffer.size() + 7) / 8  << " bytes" << std::endl;
  std::cerr << "output buffer size: " << out->tellp() << " bytes" << std::endl;
  */

  // close the output stream
  if (out != &std::cout) {
    delete out;
  }

  return 0;
}
