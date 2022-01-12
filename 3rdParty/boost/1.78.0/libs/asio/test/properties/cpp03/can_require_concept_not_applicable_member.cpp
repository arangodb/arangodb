//
// cpp03/can_require_concept_not_applicable_member.cpp
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
  template <int N>
  object<N> require_concept(prop<N>) const
  {
    return object<N>();
  }
};

namespace boost {
namespace asio {
namespace traits {

template<int N, int M>
struct require_concept_member<object<N>, prop<M> >
{
  static const bool is_valid = true;
  static const bool is_noexcept = true;
  typedef object<M> result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

int main()
{
  assert((!boost::asio::can_require_concept<object<1>, prop<2> >::value));
  assert((!boost::asio::can_require_concept<const object<1>, prop<2> >::value));
}
