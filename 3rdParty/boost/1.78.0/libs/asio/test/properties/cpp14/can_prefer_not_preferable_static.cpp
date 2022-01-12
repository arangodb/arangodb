//
// cpp14/can_prefer_not_preferable_static.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
};

int main()
{
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<1>>, "");
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<1>, prop<1>>, "");
  static_assert(!boost::asio::can_prefer_v<object<1>, prop<1>, prop<1>, prop<1>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<1>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<1>, prop<1>>, "");
  static_assert(!boost::asio::can_prefer_v<const object<1>, prop<1>, prop<1>, prop<1>>, "");
}
