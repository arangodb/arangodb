//
// cpp14/query_free.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/query.hpp>
#include <cassert>

struct prop
{
  template <typename> static constexpr bool is_applicable_property_v = true;
};

struct object
{
  friend constexpr int query(const object&, prop) { return 123; }
};

int main()
{
  object o1 = {};
  int result1 = boost::asio::query(o1, prop());
  assert(result1 == 123);
  (void)result1;

  const object o2 = {};
  int result2 = boost::asio::query(o2, prop());
  assert(result2 == 123);
  (void)result2;

  constexpr object o3 = {};
  constexpr int result3 = boost::asio::query(o3, prop());
  assert(result3 == 123);
  (void)result3;
}
