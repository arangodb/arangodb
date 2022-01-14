//
// cpp14/require_concept_member.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
  template <typename> static constexpr bool is_applicable_property_v = true;
  static constexpr bool is_requirable_concept = true;
};

template <int>
struct object
{
  template <int N>
  constexpr object<N> require_concept(prop<N>) const
  {
    return object<N>();
  }
};

int main()
{
  object<1> o1 = {};
  object<2> o2 = boost::asio::require_concept(o1, prop<2>());
  (void)o2;

  const object<1> o3 = {};
  object<2> o4 = boost::asio::require_concept(o3, prop<2>());
  (void)o4;

  constexpr object<2> o5 = boost::asio::require_concept(object<1>(), prop<2>());
  (void)o5;
}
