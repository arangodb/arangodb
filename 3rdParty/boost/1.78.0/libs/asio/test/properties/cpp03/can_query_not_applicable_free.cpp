//
// cpp03/can_query_not_applicable_free.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
  friend int query(const object&, prop) { return 123; }
};

namespace boost {
namespace asio {
namespace traits {

template<>
struct query_free<object, prop>
{
  static const bool is_valid = true;
  static const bool is_noexcept = true;
  typedef int result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

int main()
{
  assert((!boost::asio::can_query<object, prop>::value));
  assert((!boost::asio::can_query<const object, prop>::value));
}
