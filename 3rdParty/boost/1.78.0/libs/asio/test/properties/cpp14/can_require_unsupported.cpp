//
// cpp14/can_require_unsupported.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

namespace boost {
namespace asio {

template<int N, int M>
struct is_applicable_property<object<N>, prop<M> >
{
  static constexpr bool value = true;
};

} // namespace asio
} // namespace boost

int main()
{
  static_assert(!boost::asio::can_require_v<object<1>, prop<2>>, "");
  static_assert(!boost::asio::can_require_v<object<1>, prop<2>, prop<3>>, "");
  static_assert(!boost::asio::can_require_v<object<1>, prop<2>, prop<3>, prop<4>>, "");
  static_assert(!boost::asio::can_require_v<const object<1>, prop<2>>, "");
  static_assert(!boost::asio::can_require_v<const object<1>, prop<2>, prop<3>>, "");
  static_assert(!boost::asio::can_require_v<const object<1>, prop<2>, prop<3>, prop<4>>, "");
}
