#ifndef bitstream_h
#define bitstream_h

#include <iomanip>
#include <iostream>

#include "log2.h"

/* stream-like class that supports insertion and extraction bit by bit or byte by byte
 */

// little endian implementation: the "first" bit is the LSB

class bitstream {
public:
  using size_type = uint64_t;
  using block_type = uint32_t;
  using value_type = bool;

  static constexpr size_type block_size = sizeof(block_type) * 8;      // number of bits in a block
  static constexpr size_type block_bits = constexpr_log2(block_size);  // number of bits in the size used to count the bits within a block

  // bitmasks corresponing to the bits used to count ... 
  static constexpr size_type bitmask_bits = static_cast<size_type>(block_size - 1);  // ... the bits within a block
  static constexpr size_type bitmask_blocks = ~bitmask_bits;                         // ... the number of blocks

  // how many blocks are needed to store `bit_count` bits
  static constexpr size_type to_block_count(size_type bit_count) { return (bit_count + block_size - 1) / block_size; }

  // how many bits can be stored in `block_count` blocks
  static constexpr size_type to_bit_size(size_type block_count) { return block_count * block_size; }

  // constants used to initialise new blocks
  static constexpr block_type all_zeroes = 0;          // all zeroes
  static constexpr block_type all_ones = ~all_zeroes;  // all ones

  // default constructor
  bitstream() : buffer_(), size_(0)  {}

  // construct a stream with `bitcount` bits set to `value`
  bitstream(size_type bitcount, value_type value = 0) : buffer_(to_block_count(bitcount), value ? all_ones : all_zeroes), size_(bitcount) {}

  // construct a stream from pair of input iterators to bytes
  template <typename InputIt, typename = std::enable_if_t<std::is_convertible_v<std::iterator_traits<InputIt>::value_type, block_type>>>
  bitstream(InputIt first, InputIt last) : buffer_(first, last), size_(to_bit_size(buffer_.size())), write_index_(0) {}

  // reserve space in the underlying buffer for `bitcount` bits
  void reserve(size_type bitcount) {
    buffer_.resize(to_block_count(bitcount));
  }

  //
  size_type size() const { return size_; }

  // reset and clear the stream
  void reset() {
    buffer_.clear();
    size_ = 0;
    write_index_ = 0;
    read_index_ = 0;
  }

  // write one bit to the bitstream
  void write(value_type value) {
    // check if the underlying buffer_ needs to be resized
    if (write_index_ + 1 > size_) {
      size_ = write_index_ + 1;
      if (to_bit_size(buffer_.size()) < size_) {
        buffer_.resize(to_block_count(size_));
      }
    }
    // write the new bit to the position identified by write_index_
    block_type mask = 1 << (write_index_ & bitmask_bits);
    if (value) {
      buffer_[write_index_ >> block_bits] |= mask;
    } else {
      buffer_[write_index_ >> block_bits] &= ~mask;
    }
    // increase the write_index_
    ++write_index_;
  }

  // append the first `count` bits from `value`
  template <typename T>
  void write(size_type count, T value) {
    // check that the input parameters are valid
    assert(count <= sizeof(T) * 8);
    if (count == 0) {
      return;
    }
    // check if the underlying buffer_ needs to be resized
    if (write_index_ + count > size_) {
      size_ = write_index_ + count;
      if (to_bit_size(buffer_.size()) < size_) {
        buffer_.resize(to_block_count(size_));
      }
    }
    // write the new bits starting from the position identified by write_index_
    // check if there is a partially filled block
    if (size_type partial = write_index_ & bitmask_bits; partial > 0) {
      // count how many bits can be written there, up to the input size
      size_type available = std::min(block_size - partial, count);
      // write the first `available` bits to the position identified by write_index_
      block_type mask = ((1 << (partial + available)) - 1) & ~ ((1 << partial) - 1);
      buffer_[write_index_ >> block_bits] &= ~ mask;
      buffer_[write_index_ >> block_bits] |= (static_cast<block_type>(value & ((1 << available) - 1))) << partial;
      // increase the write_index_ and remove the inserted bits from the input
      write_index_ += available;
      value >>= available;
      count -= available;
    }
    assert(count == 0 or (write_index_ & bitmask_bits) == 0);
    // write the next bits one block at a time
    while (count >= block_size) {
      buffer_[write_index_ >> block_bits] = static_cast<block_type>(value & all_ones);
      // increase the write_index_ and remove the inserted bits from the input
      write_index_ += block_size;
      value >>= block_size;
      count -= block_size;
    }
    // partially fill the next block with the last bits
    if (count > 0) {
      block_type mask = (1 << (count)) - 1;
      buffer_[write_index_ >> block_bits] &= ~ mask;
      buffer_[write_index_ >> block_bits] |= static_cast<block_type>(value & mask);
      // increase the write_index_ and remove the inserted bits from the input
      write_index_ += count;
      value >>= count;
      count = 0;
    }
  }

  // FIXME
  std::vector<block_type> blocks() const {
    std::vector<block_type> buffer(buffer_.begin(), buffer_.end());
    return buffer;
  }

  // FIXME
  std::vector<char> bytes() const {
    size_type size = (size_ + 7) / 8;
    std::vector<char> buffer(size);
    for (size_type i = 0; i < size; ++i) {
      buffer[i] = static_cast<char>((buffer_[i / 4] >> ((i % 4) * 8)) & 0xFF);
    }
    return buffer;
  }

private:
  std::deque<block_type> buffer_;
  size_type size_;
  size_type write_index_ = 0;
  size_type read_index_ = 0;
};

#endif  // bitstream_h
