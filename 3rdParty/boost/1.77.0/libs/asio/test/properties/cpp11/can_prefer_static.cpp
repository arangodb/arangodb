//
// cpp11/can_prefer_static.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
};

namespace boost {
namespace asio {

template<int N, int M>
struct is_applicable_property<object<N>, prop<M> >
{
  static constexpr bool value = true;
};

namespace traits {

template<int N>
struct static_require<object<N>, prop<N> >
{
  static constexpr bool is_valid = true;
};

} // namespace traits
} // namespace asio
} // namespace boost

int main()
{
  static_assert(boost::asio::can_prefer<object<1>, prop<1>>::value, "");
  static_assert(boost::asio::can_prefer<object<1>, prop<1>, prop<1>>::value, "");
  static_assert(boost::asio::can_prefer<object<1>, prop<1>, prop<1>, prop<1>>::value, "");
  static_assert(boost::asio::can_prefer<const object<1>, prop<1>>::value, "");
  static_assert(boost::asio::can_prefer<const object<1>, prop<1>, prop<1>>::value, "");
  static_assert(boost::asio::can_prefer<const object<1>, prop<1>, prop<1>, prop<1>>::value, "");
}
