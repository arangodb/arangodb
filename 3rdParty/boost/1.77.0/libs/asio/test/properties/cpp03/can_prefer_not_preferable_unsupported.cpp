//
// cpp03/can_prefer_not_preferable_unsupported.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
  static const bool is_preferable = false;
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
  static const bool value = true;
};

} // namespace asio
} // namespace boost

int main()
{
  assert((!boost::asio::can_prefer<object<1>, prop<2> >::value));
  assert((!boost::asio::can_prefer<object<1>, prop<2>, prop<3> >::value));
  assert((!boost::asio::can_prefer<object<1>, prop<2>, prop<3>, prop<4> >::value));
  assert((!boost::asio::can_prefer<const object<1>, prop<2> >::value));
  assert((!boost::asio::can_prefer<const object<1>, prop<2>, prop<3> >::value));
  assert((!boost::asio::can_prefer<const object<1>, prop<2>, prop<3>, prop<4> >::value));
}
