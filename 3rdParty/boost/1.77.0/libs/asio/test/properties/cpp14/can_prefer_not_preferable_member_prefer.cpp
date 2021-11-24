//
// cpp14/can_prefer_not_preferable_member_prefer.cpp
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
  template <typename> static constexpr bool is_applicable_property_v = true;
  static constexpr bool is_preferable = false;
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
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<2>>, "");
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<2>, prop<3>>, "");
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<2>, prop<3>, prop<4>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<2>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<2>, prop<3>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<2>, prop<3>, prop<4>>, "");
}
