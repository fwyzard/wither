// encode an arbitrary input via 1-byte Huffman coding and output the result

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include <boost/dynamic_bitset.hpp>

#include <fmt/printf.h>

using alphabet_type = unsigned char;  // type of the "charachters" that compose the alphabet of the input data
constexpr int alphabet_bits = 8;      // number of bits needed to encode one input character
constexpr int alphabet_size = 256;    // number of different characters that make up the input alphabet

using encoded_type = boost::dynamic_bitset<uint16_t>;
struct code_point {
  alphabet_type value;
  encoded_type code;
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
  float weights[alphabet_size];
  for (int i = 0; i < alphabet_size; ++i) {
    weights[i] = 0.;
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
  std::vector<alphabet_type> buffer(std::istreambuf_iterator<char>(*in), {});
  std::cerr << "read " << buffer.size() << " " << alphabet_bits << "-bit characters" << std::endl;
  std::cerr << std::endl;

  // close the input stream
  if (in != &std::cin) {
    delete in;
  }

  // measure the frequency for the input data
  for (alphabet_type character : buffer) {
    ++weights[character];
  }

  /*
  // print the frequency of each character
  for (int i = 0; i < alphabet_size; ++i) {
    std::cerr << representation(i) << ": " << weights[i] / buffer.size() << std::endl;
  }
  std::cerr << std::endl;
  */

  struct node_type {
    node_type() = default;
    node_type(float weight) : weight(weight) {}

    node_type* parent = nullptr;  // nullptr if no parent
    float weight = 0.;            // cumulative weight
    char bit = -1;                // 0 or 1, assigned while the tree is being built
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
  std::priority_queue queue{[](node_type const* left, node_type const* right) { return left->weight > right->weight; }, tmp};
  tmp.clear();

  // combine the two lowest-weight nodes form the queue to create a new node with the sum of their weight
  node_type* root;
  while (queue.size() > 1) {
    node_type* first = queue.top();
    queue.pop();
    node_type* second = queue.top();
    queue.pop();
    node_type* node = &nodes.emplace_back(first->weight + second->weight);
    first->parent = node;
    first->bit = 0;
    second->parent = node;
    second->bit = 1;
    if (queue.empty()) {
      // the node is the root of the tree
      root = node;
    } else {
      // add the intermediate node back to the queue
      queue.push(node);
    }
  }

  // check that the queue is empty and the tree has the expected size
  assert(queue.empty());
  assert(nodes.size() == alphabet_size * 2 - 1);

  // traverse the tree and build the explicit encoding
  std::vector<code_point> huffman_code;
  huffman_code.resize(alphabet_size);

  int depth;
  int min_depth = alphabet_size;
  int max_depth = 0;
  encoded_type buf(alphabet_size, 0);
  for (int i = 0; i < alphabet_size; ++i) {
    // make sure the temporary buffer can hold any possible Huffman coding over the given alphabet
    buf.resize(alphabet_size);
    buf.reset();
    depth = 0;
    // start from the leaf and traverse the tree until the root
    for (node_type const* node = &nodes[i]; node != root; node = node->parent) {
      // non-root nodes have a bit value of 0 or 1
      buf[depth] = node->bit;
      ++depth;
    }
    // resize the temporary buffer to the length of the Huffman coding for this character
    buf.resize(depth);
    huffman_code[i] = {(alphabet_type)i, buf};

    // keep track of the minimum and maximum length of all the Huffman codes
    if (depth > min_depth) {
      min_depth = depth;
    }
    if (depth < max_depth) {
      max_depth = depth;
    }
  }

  // print the weight, frequency and Huffman code of each byte
  std::cerr << "Huffman coding" << std::endl;
  for (auto const& point : huffman_code) {
    auto weight = weights[point.value];
    std::string code;
    to_string(point.code, code);
    std::cerr << fmt::sprintf("%s: %10.0f (%8.4f) \"%s\"", representation(point.value), weight, weight / buffer.size(), code) << std::endl;
  }
  std::cerr << std::endl;

  // convert to a canonical Huffman coding

  // 1. sort the Huffman codes by length
  std::sort(huffman_code.begin(), huffman_code.end(), [](code_point const& left, code_point const& right) { return left.code.size() == right.code.size() ? left.value < right.value : left.code.size() < right.code.size(); });

  // 2. renumber the codes starting for 0
  encoded_type last_code;
  for (auto& point : huffman_code) {
    if (last_code.empty()) {
      // first code point: set the Huffman coding to all 0
      point.code.reset();
      last_code = point.code;
    } else {
      // next code point: increment the value of the last code point
      std::vector<encoded_type::block_type> value(point.code.num_blocks(), 0);
      to_block_range(last_code, value.begin());
      for (auto& block: value) {
        // increment the first block, or if there was an overflow
        block +=1;
        // if the addition did not overflow, do not increment the next blocks
        if (block != 0) break;
      }
      from_block_range(value.begin(), value.end(), last_code);

      // if the new code point is larger than the previous one, extend (left shift) as needed
      if (auto delta = point.code.size() - last_code.size(); delta > 0) {
        last_code.resize(point.code.size());
        last_code <<= delta;
      }

      // update the code point
      point.code = last_code;
    }
  }

  // print the weight, frequency and canonical Huffman coding
  std::cerr << "canonical Huffman coding" << std::endl;
  for (auto const& point : huffman_code) {
    auto weight = weights[point.value];
    std::string code;
    to_string(point.code, code);
    std::cerr << fmt::sprintf("%s: %10.0f (%8.4f) \"%s\"", representation(point.value), weight, weight / buffer.size(), code) << std::endl;
  }
  std::cerr << std::endl;

  // close the output stream
  if (out != &std::cout) {
    delete out;
  }

  return 0;
}
