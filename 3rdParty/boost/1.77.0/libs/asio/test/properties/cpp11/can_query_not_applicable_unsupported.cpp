//
// cpp11/can_query_not_applicable_unsupported.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

int main()
{
  static_assert(!boost::asio::can_query<object, prop>::value, "");
  static_assert(!boost::asio::can_query<const object, prop>::value, "");
}
