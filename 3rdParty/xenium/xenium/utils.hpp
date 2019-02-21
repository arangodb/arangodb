//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_UTILS_HPP
#define XENIUM_UTILS_HPP

namespace xenium { namespace utils {
  template <typename T>
  constexpr bool is_power_of_two(T val) {
    return (val & (val - 1)) == 0;
  }

  template <typename T>
  constexpr unsigned find_last_bit_set(T val) {
    unsigned result = 0;
    for (; val != 0; val >>= 1)
      ++result;
    return result;
  }

  template <typename T>
  constexpr T next_power_of_two(T val)
  {
    if (is_power_of_two(val))
      return val;

    return 1ul << find_last_bit_set(val);
  }

  template <typename T>
  struct modulo {
    T operator()(T a, T b) { return a % b; }
  };
}}
#endif
