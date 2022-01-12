//
// start.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/execution/start.hpp>

#include <boost/system/error_code.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

static int call_count = 0;

struct no_start
{
};

struct const_member_start
{
  void start() const BOOST_ASIO_NOEXCEPT
  {
    ++call_count;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct start_member<const const_member_start>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

struct free_start_const_receiver
{
  friend void start(const free_start_const_receiver&) BOOST_ASIO_NOEXCEPT
  {
    ++call_count;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_FREE_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct start_free<const free_start_const_receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_FREE_TRAIT)

struct non_const_member_start
{
  void start() BOOST_ASIO_NOEXCEPT
  {
    ++call_count;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct start_member<non_const_member_start>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

struct free_start_non_const_receiver
{
  friend void start(free_start_non_const_receiver&) BOOST_ASIO_NOEXCEPT
  {
    ++call_count;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_FREE_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct start_free<free_start_non_const_receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_FREE_TRAIT)

void test_can_start()
{
  BOOST_ASIO_CONSTEXPR bool b1 = exec::can_start<
      no_start&>::value;
  BOOST_ASIO_CHECK(b1 == false);

  BOOST_ASIO_CONSTEXPR bool b2 = exec::can_start<
      const no_start&>::value;
  BOOST_ASIO_CHECK(b2 == false);

  BOOST_ASIO_CONSTEXPR bool b3 = exec::can_start<
      const_member_start&>::value;
  BOOST_ASIO_CHECK(b3 == true);

  BOOST_ASIO_CONSTEXPR bool b4 = exec::can_start<
      const const_member_start&>::value;
  BOOST_ASIO_CHECK(b4 == true);

  BOOST_ASIO_CONSTEXPR bool b5 = exec::can_start<
      free_start_const_receiver&>::value;
  BOOST_ASIO_CHECK(b5 == true);

  BOOST_ASIO_CONSTEXPR bool b6 = exec::can_start<
      const free_start_const_receiver&>::value;
  BOOST_ASIO_CHECK(b6 == true);

  BOOST_ASIO_CONSTEXPR bool b7 = exec::can_start<
      non_const_member_start&>::value;
  BOOST_ASIO_CHECK(b7 == true);

  BOOST_ASIO_CONSTEXPR bool b8 = exec::can_start<
      const non_const_member_start&>::value;
  BOOST_ASIO_CHECK(b8 == false);

  BOOST_ASIO_CONSTEXPR bool b9 = exec::can_start<
      free_start_non_const_receiver&>::value;
  BOOST_ASIO_CHECK(b9 == true);

  BOOST_ASIO_CONSTEXPR bool b10 = exec::can_start<
      const free_start_non_const_receiver&>::value;
  BOOST_ASIO_CHECK(b10 == false);
}

void increment(int* count)
{
  ++(*count);
}

void test_start()
{
  call_count = 0;
  const_member_start ex1 = {};
  exec::start(ex1);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const const_member_start ex2 = {};
  exec::start(ex2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::start(const_member_start());
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  free_start_const_receiver ex3 = {};
  exec::start(ex3);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const free_start_const_receiver ex4 = {};
  exec::start(ex4);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::start(free_start_const_receiver());
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  non_const_member_start ex5 = {};
  exec::start(ex5);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  free_start_non_const_receiver ex6 = {};
  exec::start(ex6);
  BOOST_ASIO_CHECK(call_count == 1);
}

BOOST_ASIO_TEST_SUITE
(
  "start",
  BOOST_ASIO_TEST_CASE(test_can_start)
  BOOST_ASIO_TEST_CASE(test_start)
)
