//
// cpp03/query_static.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
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
};

struct object
{
};

namespace boost {
namespace asio {

template<>
struct is_applicable_property<object, prop>
{
  static const bool value = true;
};

namespace traits {

template<>
struct static_query<object, prop>
{
  static const bool is_valid = true;
  static const bool is_noexcept = true;
  typedef int result_type;
  static int value() { return 123; }
};

} // namespace traits
} // namespace asio
} // namespace boost

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
}
