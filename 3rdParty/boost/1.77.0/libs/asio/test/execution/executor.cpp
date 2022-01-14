//
// executor.cpp
// ~~~~~~~~~~~~
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
#include <boost/asio/execution/executor.hpp>

#include "../unit_test.hpp"

struct not_an_executor
{
};

struct executor
{
  executor()
  {
  }

  executor(const executor&) BOOST_ASIO_NOEXCEPT
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  executor(executor&&) BOOST_ASIO_NOEXCEPT
  {
  }
#endif // defined(BOOST_ASIO_HAS_MOVE)

  template <typename F>
  void execute(BOOST_ASIO_MOVE_ARG(F) f) const BOOST_ASIO_NOEXCEPT
  {
    (void)f;
  }

  bool operator==(const executor&) const BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  bool operator!=(const executor&) const BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename F>
struct execute_member<executor, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <>
struct equality_comparable<executor>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void is_executor_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_executor<
        void
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_executor<
        not_an_executor
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_executor<
        executor
      >::value));
}

void is_executor_of_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_executor_of<
        void,
        void(*)()
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_executor_of<
        not_an_executor,
        void(*)()
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_executor_of<
        executor,
        void(*)()
      >::value));
}

struct executor_with_other_shape_type
{
  typedef double shape_type;
};

void executor_shape_test()
{
  BOOST_ASIO_CHECK((
      boost::asio::is_same<
        boost::asio::execution::executor_shape<executor>::type,
        std::size_t
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::is_same<
        boost::asio::execution::executor_shape<
          executor_with_other_shape_type
        >::type,
        double
      >::value));
}

struct executor_with_other_index_type
{
  typedef unsigned char index_type;
};

void executor_index_test()
{
  BOOST_ASIO_CHECK((
      boost::asio::is_same<
        boost::asio::execution::executor_index<executor>::type,
        std::size_t
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::is_same<
        boost::asio::execution::executor_index<
          executor_with_other_shape_type
        >::type,
        double
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::is_same<
        boost::asio::execution::executor_index<
          executor_with_other_index_type
        >::type,
        unsigned char
      >::value));
}

BOOST_ASIO_TEST_SUITE
(
  "executor",
  BOOST_ASIO_TEST_CASE(is_executor_test)
  BOOST_ASIO_TEST_CASE(is_executor_of_test)
  BOOST_ASIO_TEST_CASE(executor_shape_test)
  BOOST_ASIO_TEST_CASE(executor_index_test)
)
