//
// cpp11/can_require_not_applicable_unsupported.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/require.hpp>
#include <cassert>

template <int>
struct prop
{
};

template <int>
struct object
{
};

int main()
{
  static_assert(!boost::asio::can_require<object<1>, prop<2>>::value, "");
  static_assert(!boost::asio::can_require<object<1>, prop<2>, prop<3>>::value, "");
  static_assert(!boost::asio::can_require<object<1>, prop<2>, prop<3>, prop<4>>::value, "");
  static_assert(!boost::asio::can_require<const object<1>, prop<2>>::value, "");
  static_assert(!boost::asio::can_require<const object<1>, prop<2>, prop<3>>::value, "");
  static_assert(!boost::asio::can_require<const object<1>, prop<2>, prop<3>, prop<4>>::value, "");
}
