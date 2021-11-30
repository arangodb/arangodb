//
// cpp11/can_prefer_not_applicable_member_prefer.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/prefer.hpp>
#include <cassert>

template <int>
struct prop
{
  static constexpr bool is_preferable = true;
};

template <int>
struct object
{
  template <int N>
  constexpr object<N> prefer(prop<N>) const
  {
    return object<N>();
  }
};

int main()
{
  static_assert(!boost::asio::can_prefer<object<1>, prop<2>>::value, "");
  static_assert(!boost::asio::can_prefer<object<1>, prop<2>, prop<3>>::value, "");
  static_assert(!boost::asio::can_prefer<object<1>, prop<2>, prop<3>, prop<4>>::value, "");
  static_assert(!boost::asio::can_prefer<const object<1>, prop<2>>::value, "");
  static_assert(!boost::asio::can_prefer<const object<1>, prop<2>, prop<3>>::value, "");
  static_assert(!boost::asio::can_prefer<const object<1>, prop<2>, prop<3>, prop<4>>::value, "");
}
