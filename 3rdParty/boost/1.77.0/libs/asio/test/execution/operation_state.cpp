//
// operation_state.cpp
// ~~~~~~~~~~~~~~~~~~~
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
#include <boost/asio/execution/operation_state.hpp>

#include <string>
#include <boost/system/error_code.hpp>
#include "../unit_test.hpp"

struct not_an_operation_state_1
{
};

struct not_an_operation_state_2
{
  void start()
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

template <>
struct start_member<not_an_operation_state_2>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct operation_state
{
  void start() BOOST_ASIO_NOEXCEPT
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

template <>
struct start_member<operation_state>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void is_operation_state_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_operation_state<
        void
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_operation_state<
        not_an_operation_state_1
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_operation_state<
        not_an_operation_state_2
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_operation_state<
        operation_state
      >::value));
}

BOOST_ASIO_TEST_SUITE
(
  "operation_state",
  BOOST_ASIO_TEST_CASE(is_operation_state_test)
)
