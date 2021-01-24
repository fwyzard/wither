#ifndef huffman_h
#define huffman_h

#include "bitstream.h"
#include "invert.h"

class huffman_encoding {
public:

  using alphabet_type = uint8_t;             // type of the symbols that compose the alphabet of the input data
  static constexpr int alphabet_bits = 8;    // number of bits needed to encode one input symbol
  static constexpr int alphabet_size = 256;  // number of different symbols that make up the input alphabet

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
  //     64 bits   | 1 / F(67) = 1 / 44945570212853 ~ 2^ -45.4 |    40.88 TiB
  //
  // It seems safe to assume that 64 bits should suffice for all reasonable input datasets.
  //
  // A length-limited Huffman coding can be further restricted to have all symbols envoded by a smaller, fixed number of bits.

  struct encoded_type {
    using value_type = uint64_t;
    using size_type = uint8_t;

    value_type value = 0;
    size_type size = 0;
  };

  std::string to_string(encoded_type::value_type value, encoded_type::size_type bits) const {
    std::string out(bits + 2, '\0');
    out[0] = '0';
    out[1] = 'b';
    for (encoded_type::size_type i = 0; i < bits; ++i) {
      out[i + 2] = (value & (0x01 << i) ? '1' : '0');
    }
    return out;
  }

  struct code_point {
    alphabet_type value = 0;
    encoded_type code = {};
  };


  // default constructor: build a "neutral" Huffman coding that encodes each symbol with itself
  huffman_encoding() {
    // initialise the weights to zero, and the Huffman coding to a neutral one
    for (int i = 0; i < alphabet_size; ++i) {
      weights_[i] = 0;
      lengths_[i] = alphabet_bits;
      encoding_[i] = invert_bits(static_cast<uint8_t>(i), alphabet_bits);
    }
  }


  // constructor from the full dataset
  huffman_encoding(alphabet_type const* data, size_t size) : huffman_encoding() {
    // scan the whole input
    scan_input(data, size);

    // compute the optimal code length for each symbol based on the weights
    get_code_lenghts_from_data();

    // build the canonical Huffman coding from the code lengths
    build_canonical_coding();
  }


  void scan_input(alphabet_type const* data, size_t size) {
    // count the input buffer size
    original_size_ += size;

    // count the occurrencies of each symbol in the input data
    for (size_t i = 0; i < size; ++i) {
      int symbol = data[i];
      ++weights_[symbol];
    }

    /*
    // print the weight and frequency for the input symbols
    for (unsigned int i = 0; i < alphabet_size; ++i) {
      auto weight = weights_[i];
      std::cerr << fmt::sprintf("0x%02x: %10d (%8.4f)", i, weight, (double)weight / original_size_) << std::endl;
    }
    std::cerr << std::endl;
    */
  }

  // compute the code length based on the weights
  void get_code_lenghts_from_data() {
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
      nodes.emplace_back(weights_[i]);
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

    // iteratively combine the two lowest-weight nodes form the queue to create a new node with the sum of their weight
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

    // traverse the tree and count the encoding bit lengths
    for (int i = 0; i < alphabet_size; ++i) {
      encoded_type::size_type length = 0;
      // start from the leaf and traverse the tree until the root
      for (node_type const* node = &nodes[i]; node != root; node = node->parent) {
        ++length;
      }
      lengths_[i] = length;
      // count how many bits are used in total by all the symbol
      encoded_size_ += weights_[i] * length;
    }

    /*
    // print the weight, frequency and canonical Huffman coding
    std::cerr << "Huffman coding" << std::endl;
    for (unsigned int i = 0; i < alphabet_size; ++i) {
      auto weight = weights_[i];
      std::cerr << fmt::sprintf("0x%02x: %10d (%8.4f) \"%s\"", i, weight, (double)weight / original_size_, to_string(encoding_[i], lengths_[i])) << std::endl;
    }
    std::cerr << std::endl;
    */
  }


  // build the canonical Huffman coding from the legths of the encoding of each symbol
  void build_canonical_coding() {

    // 1. fill the symbols and their Huffman code lengths in a single variable
    static_assert(sizeof(encoded_type::size_type) <= 2);
    static_assert(sizeof(alphabet_type) <= 2);
    using pair_t = uint32_t;

    pair_t sortable[alphabet_size];
    for (int i = 0; i < alphabet_size; ++i) {
      sortable[i] = static_cast<uint16_t>(lengths_[i]) << 16 | static_cast<uint16_t>(static_cast<alphabet_type>(i));
    }

    // 2. sort the Huffman codes by length, and then by symbol
    std::sort(sortable, sortable + alphabet_size);

    // 3. assign an encoding to each symbol, starting from 0

    alphabet_type symbol;
    encoded_type::size_type prev_size, size;
    encoded_type::value_type value;
    for (int i = 0; i < alphabet_size; ++i) {
      // read the symbol and the encoding legth
      symbol = static_cast<alphabet_type>(sortable[i] & 0xFFFF);
      size = static_cast<encoded_type::size_type>((sortable[i] >> 16) & 0xFFFF);

      if (i == 0) {
        // first code point: set the Huffman coding to all 0
        value = 0;
        prev_size = size;
      } else {
        // next code point: increment the value
        ++value;

        // if the new code point is longer than the previous one, extend (left shift) as needed
        if (size > prev_size) {
          value <<= (size - prev_size);
          prev_size = size;
        }
      }

      // store the coding in little-endian format, with the first bit of the Huffman encoding in the LSB
      lengths_[static_cast<uint8_t>(symbol)] = size;
      encoding_[static_cast<uint8_t>(symbol)] = invert_bits(value, size);
    }

    /*
    // print the weight, frequency and Huffman coding
    std::cerr << "canonical Huffman coding" << std::endl;
    for (unsigned int i = 0; i < alphabet_size; ++i) {
      auto weight = weights_[i];
      std::cerr << fmt::sprintf("0x%02x: %10d / %10d (%8.4f) \"%s\"", i, weight, original_size_, (double)weight / original_size_, to_string(encoding_[i], lengths_[i])) << std::endl;
    }
    std::cerr << std::endl;
    */
  }


  // serialise the canonical Huffman coding to a bit stream
  void serialise(bitstream& stream) const {

    // encode the header
    stream.write(64, header_size_ + encoded_size_);
    stream.write(64, original_size_);

    // encode the canonical Huffman coding
    stream.write(16, alphabet_size);
    for (encoded_type::size_type bits: lengths_) {
      // 6 bit per symbol: encode the size of each symbol's coding - 1
      stream.write(6, bits - 1);
    }

  }


  /*
  // deserialise the canonical Huffman coding to a bit stream
  void deserialise(bitstream& stream) {

    // read the size (in bits) of the header and encoded message
    uint64_t message_size = stream.read(64);

    // read the size (in symbols) of the decoded message
    original_size_ = stream.read(64);

    // check that the alphabet size matches the Huffman coding
    uint16_t read_alphabet_size = stream.read(16);
    assert(alphabet_size == read_alphabet_size);

    // size of the encoded message (in bits)
    encoded_size_ = message_size = header_size_;

    // read the size (in bits, - 1) of each symbol's coding
    for (encoded_type::size_type & bits: lengths_) {
      bits = stream.read(6);
    }

    // build the canonical Huffman coding from the legths of the encoding of each symbol
    build_canonical_coding();
  }
  */


public:
  // size of the header and of the message
  uint64_t header_size_ = 64                  // 64 bit:            encode the encoded message size, including the header itself, in bits
                        + 64                  // 64 bit:            encode the original message size, in symbols
                        + 16                  // 16 bit:            encode the number of symbols in the alphabet
                                              //                    [NB: could be replaced by alphabet_bits - 1]
                        + 6 * alphabet_size;  //  6 bit per symbol: encode the size of each symbol's coding - 1

  // size of the decoded (original) message (in symbols), and of the encodoed version (in bits)
  uint64_t original_size_ = 0;
  uint64_t encoded_size_ = 0;

  // weight for each symbol, proportional to the frequency with which they are found in the input
  weight_type weights_[alphabet_size];

  // Huffman encoding of each symbol
  encoded_type::size_type lengths_[alphabet_size];
  encoded_type::value_type encoding_[alphabet_size];

  // inverse mapping to speed up decoding
  //...
};

#endif  // huffman_h
