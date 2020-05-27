// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <random>

using uniform = std::uniform_real_distribution<>;
using uniform_int = std::uniform_int_distribution<>;
using normal = std::normal_distribution<>;

template <class Distribution, class... Ts>
Distribution init(Ts...);

template <>
uniform init<uniform>() {
  return uniform{0.0, 1.0};
}

template <>
normal init<normal>() {
  return normal{0.5, 0.3};
}

template <>
uniform_int init<uniform_int, int>(int n) {
  return uniform_int{0, n};
}

template <class Distribution, std::size_t N = 1 << 15>
struct generator {
  template <class... Ts>
  generator(Ts... ts) {
    std::default_random_engine rng(1);
    auto dis = init<Distribution>(ts...);
    std::generate(buffer_, buffer_ + N, [&] { return dis(rng); });
  }

  const double& operator()() {
    ++ptr_;
    if (ptr_ == buffer_ + N) ptr_ = buffer_;
    return *ptr_;
  }

  double buffer_[N];
  const double* ptr_ = buffer_ - 1;
};
