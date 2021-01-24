// encode an arbitrary input via 1-byte Huffman coding and output the result

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
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
  // FIXME this needs to be rewritten to support non-8 bit characters
  std::vector<uint8_t> input_buffer(std::istreambuf_iterator<char>(*in), {});

  // close the input stream
  if (in != &std::cin) {
    delete in;
  }

  // build the canonical Huffman coding for the input
  huffman_encoding encoding(input_buffer.data(), input_buffer.size());

  // bitstream used to encode the input according to the canonical Huffman coding
  bitstream encoding_buffer;
  encoding_buffer.reserve(encoding.header_size_ + encoding.encoded_size_);

  // write the canonical Huffman coding to the output buffer
  encoding.serialise(encoding_buffer);

  // encode the input according to the Huffman coding
  for (auto symbol : input_buffer) {
    huffman_encoding::encoded_type::size_type size = encoding.lengths_[static_cast<size_t>(symbol)];
    huffman_encoding::encoded_type::value_type value = encoding.encoding_[static_cast<size_t>(symbol)];
    encoding_buffer.write(size, value);
  }

  std::vector<char> output_buffer = encoding_buffer.bytes();

  out->write((const char*) output_buffer.data(), output_buffer.size());
  out->flush();
  std::cerr << "input buffer size:  " << input_buffer.size() << " " << huffman_encoding::alphabet_bits << "-bit characters" << std::endl;
  std::cerr << "output buffer size: " << (encoding.header_size_ + encoding.encoded_size_ + 7) / 8 << " bytes" << std::endl;
  std::cerr << "output buffer size: " << (encoding_buffer.size() + 7) / 8  << " bytes" << std::endl;
  std::cerr << "output buffer size: " << out->tellp() << " bytes" << std::endl;

  // close the output stream
  if (out != &std::cout) {
    delete out;
  }

  return 0;
}
