#ifndef log2_h
#define log2_h

#include <limits>

template <typename T>
constexpr T constexpr_log2(T value) {
  return value == 0 ? std::numeric_limits<T>::lowest() : value == 1 ? 0 : 1 + constexpr_log2(value / 2);
}

#endif  // log2_h
