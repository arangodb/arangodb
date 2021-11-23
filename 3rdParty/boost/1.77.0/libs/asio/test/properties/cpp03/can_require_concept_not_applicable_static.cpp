//
// cpp03/can_require_concept_not_applicable_static.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/require_concept.hpp>
#include <cassert>

template <int>
struct prop
{
  static const bool is_requirable_concept = true;
};

template <int>
struct object
{
};

namespace boost {
namespace asio {
namespace traits {

template<int N>
struct static_require_concept<object<N>, prop<N> >
{
  static const bool is_valid = true;
};

} // namespace traits
} // namespace asio
} // namespace boost

int main()
{
  assert((!boost::asio::can_require_concept<object<1>, prop<2> >::value));
  assert((!boost::asio::can_require_concept<const object<1>, prop<2> >::value));
}
