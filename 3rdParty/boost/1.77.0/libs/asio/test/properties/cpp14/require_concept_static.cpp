//
// cpp14/require_concept_static.cpp
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
  template <typename> static constexpr bool static_query_v = true;
  static constexpr bool value() { return true; }
};

template <int>
struct object
{
};

int main()
{
  object<1> o1 = {};
  const object<1>& o2 = boost::asio::require_concept(o1, prop<1>());
  assert(&o1 == &o2);
  (void)o2;

  const object<1> o3 = {};
  const object<1>& o4 = boost::asio::require_concept(o3, prop<1>());
  assert(&o3 == &o4);
  (void)o4;

  constexpr object<1> o5 = boost::asio::require_concept(object<1>(), prop<1>());
  (void)o5;
}
