//
// cpp14/require_static.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/require.hpp>
#include <cassert>

template <int>
struct prop
{
  template <typename> static constexpr bool is_applicable_property_v = true;
  static constexpr bool is_requirable = true;
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
  object<1> o2 = boost::asio::require(o1, prop<1>());
  object<1> o3 = boost::asio::require(o1, prop<1>(), prop<1>());
  object<1> o4 = boost::asio::require(o1, prop<1>(), prop<1>(), prop<1>());
  (void)o2;
  (void)o3;
  (void)o4;

  const object<1> o5 = {};
  object<1> o6 = boost::asio::require(o5, prop<1>());
  object<1> o7 = boost::asio::require(o5, prop<1>(), prop<1>());
  object<1> o8 = boost::asio::require(o5, prop<1>(), prop<1>(), prop<1>());
  (void)o6;
  (void)o7;
  (void)o8;

  constexpr object<1> o9 = boost::asio::require(object<1>(), prop<1>());
  constexpr object<1> o10 = boost::asio::require(object<1>(), prop<1>(), prop<1>());
  constexpr object<1> o11 = boost::asio::require(object<1>(), prop<1>(), prop<1>(), prop<1>());
  (void)o9;
  (void)o10;
  (void)o11;
}
