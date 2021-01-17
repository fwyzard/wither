// encode an arbitrary input via 1-byte Huffman coding and output the result

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include <fmt/printf.h>

namespace {

  template <typename T>
  constexpr T constexpr_log2(T value) {
    return value == 0 ? std::numeric_limits<T>::lowest() : value == 1 ? 0 : 1 + constexpr_log2(value / 2);
  }
}  // namespace

using alphabet_type = unsigned char;  // type of the symbols that compose the alphabet of the input data
constexpr int alphabet_bits = 8;      // number of bits needed to encode one input symbol
constexpr int alphabet_size = 256;    // number of different symbols that make up the input alphabet

using weight_type = uint64_t;  // type used to store the weight of each symbol

// According to
//   Abu-Mostafa, Y.S. (California Institute of Technology), and R. J. McEliece, "Maximal Codeword Lengths in Huffman Codes",
//   The Telecommunications and Data Acquisition Progress Report 42-110: April-June 1992, pp. 188-193, August 15, 1992.
// a Huffman coding is limited to N bits if the smallest probability to encode is greater than 1 / F(N+3), where F(K) is the K-th Fibonacci number.
//                               1
// Thus, for N = 16 bits, p > ------- = 1 / 4181 = 0.0023917...
//                             F(19)
//
// In turn, this can only happen if there are more than 4181 input symbols. Thus, assuming 1-byte symbols, any byte from an input data of up to
// 4181 byts can always be encoded by at most 16 bits.
//
// Some examples:
//
//    max length | smallest probability greater than         | min input size for 1-byte symbols
//   ----------- | ----------------------------------------- | ---------------------------------
//     16 bits   | 1 / F(19) = 1 /           4181 ~ 2^ -12   |     4.08 KiB
//     24 bits   | 1 / F(27) = 1 /         196418 ~ 2^ -17.6 |   191.81 KiB
//     32 bits   | 1 / F(33) = 1 /        3524578 ~ 2^ -21.7 |     3.36 MiB
//     48 bits   | 1 / F(51) = 1 /    20365011074 ~ 2^ -34.2 |    18.97 GiB
//     64 bits   | 1 / F(67) = 1 / 44945570212853 ~ 2^ -45.4 |    48.88 TiB
//
// It seems safe to assume that 64 bits should suffice for all reasonable input datasets.
//
// A length-limited Huffman coding can be further restricted to have all symbols envoded by a smaller, fixed number of bits.

struct encoded_type {
  uint64_t value = 0;
  uint8_t size = 0;

  std::string to_string() const {
    std::string out(size, '\0');
    for (uint8_t i = 0; i < size; ++i) {
      out[i] = (value & (0x01 << (size - i - 1)) ? '1' : '0');
    }
    return out;
  }
};

struct code_point {
  alphabet_type value = 0;
  encoded_type code = {};
};

std::string representation(alphabet_type i) {
  static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  int chars = (alphabet_bits + 3) / 4;  // round up to the number of hex characters
  std::string out(chars + 2, '\0');
  out[0] = '0';
  out[1] = 'x';
  while (chars > 0) {
    --chars;
    out[chars + 2] = hex[i % 16];
    i = i / 16;
  }
  return out;
}

int main(int argc, const char* argv[]) {
  // weights for each byte, proportional to the frequency with which they are found in the input
  weight_type weights[alphabet_size];
  for (int i = 0; i < alphabet_size; ++i) {
    weights[i] = 0;
  }

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
  std::vector<alphabet_type> input_buffer(std::istreambuf_iterator<char>(*in), {});
  std::cerr << "read " << input_buffer.size() << " " << alphabet_bits << "-bit characters" << std::endl;
  std::cerr << std::endl;

  // close the input stream
  if (in != &std::cin) {
    delete in;
  }

  // measure the frequency for the input data
  for (alphabet_type symbol : input_buffer) {
    ++weights[symbol];
  }

  /*
  // print the frequency of each symbol
  for (int i = 0; i < alphabet_size; ++i) {
    std::cerr << representation(i) << ": " << (double) weights[i] / input_buffer.size() << std::endl;
  }
  std::cerr << std::endl;
  */

  struct node_type {
    node_type() = default;
    node_type(weight_type weight) : weight(weight) {}

    node_type* parent = nullptr;  // nullptr if no parent
    weight_type weight = 0;       // cumulative weight
    uint32_t height = 0;          // height of the node (longest distance from a leaf)
  };

  // reserve enough space for all the nodes (leaves and intermediate nodes)
  std::vector<node_type> nodes;
  nodes.reserve(alphabet_size * 2 - 1);

  // create the leaves
  for (int i = 0; i < alphabet_size; ++i) {
    nodes.emplace_back(weights[i]);
  }

  // use a priority queue to build up the tree
  std::vector<node_type*> tmp;
  tmp.reserve(alphabet_size);
  for (int i = 0; i < alphabet_size; ++i) {
    tmp.push_back(&nodes[i]);
  }
  auto compare_nodes = [](node_type const* left, node_type const* right) { return left->weight == right->weight ? left->height > right->height : left->weight > right->weight; };
  std::priority_queue queue(compare_nodes, tmp);
  tmp.clear();

  // combine the two lowest-weight nodes form the queue to create a new node with the sum of their weight
  node_type* root;
  while (queue.size() > 1) {
    node_type* first = queue.top();
    queue.pop();
    node_type* second = queue.top();
    queue.pop();
    node_type* node = &nodes.emplace_back(first->weight + second->weight);
    node->height = std::max(first->height, second->height) + 1;
    first->parent = node;
    second->parent = node;
    if (queue.empty()) {
      // the node is the root of the tree
      root = node;
    } else {
      // add the intermediate node back to the queue
      queue.push(node);
    }
  }
  assert(root->height <= 64);

  // check that the queue is empty and the tree has the expected size
  assert(queue.empty());
  assert(nodes.size() == alphabet_size * 2 - 1);

  // traverse the tree and build the explicit encoding
  std::vector<code_point> huffman_code;
  huffman_code.resize(alphabet_size);

  encoded_type buf;
  for (int i = 0; i < alphabet_size; ++i) {
    // reset the temporary buffer
    buf.value = 0;
    buf.size = 0;
    // start from the leaf and traverse the tree until the root
    for (node_type const* node = &nodes[i]; node != root; node = node->parent) {
      ++buf.size;
    }
    huffman_code[i] = {(alphabet_type)i, buf};
  }

  // build the canonical Huffman coding

  // 1. sort the Huffman codes by length
  std::sort(huffman_code.begin(), huffman_code.end(), [](code_point const& left, code_point const& right) { return left.code.size == right.code.size ? left.value < right.value : left.code.size < right.code.size; });

  // 2. assign an encoding to each symbol, starting from 0
  encoded_type last_code;
  for (auto& point : huffman_code) {
    if (last_code.size == 0) {
      // first code point: set the Huffman coding to all 0
      point.code.value = 0;
      last_code = point.code;
    } else {
      // next code point: increment the value of the last code point
      ++last_code.value;

      // if the new code point is longer than the previous one, extend (left shift) as needed
      if (auto shift = point.code.size - last_code.size; shift > 0) {
        last_code.size = point.code.size;
        last_code.value <<= shift;
      }

      // update the code point
      point.code = last_code;
    }
  }

  // print the weight, frequency and canonical Huffman coding
  std::cerr << "canonical Huffman coding" << std::endl;
  for (auto const& point : huffman_code) {
    auto weight = weights[point.value];
    std::cerr << fmt::sprintf("%s: %10d (%8.4f) \"%s\"", representation(point.value), weight, (double)weight / input_buffer.size(), point.code.to_string()) << std::endl;
  }
  std::cerr << std::endl;

  // close the output stream
  if (out != &std::cout) {
    delete out;
  }

  return 0;
}
