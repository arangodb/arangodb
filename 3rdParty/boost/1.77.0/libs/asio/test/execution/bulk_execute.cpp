//
// bulk_execute.cpp
// ~~~~~~~~~~~~~~~~
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
#include <boost/asio/execution/bulk_execute.hpp>

#include <boost/asio/execution.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

int call_count = 0;

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

struct sender : exec::sender_base
{
  sender()
  {
  }

  template <typename R>
  operation_state connect(BOOST_ASIO_MOVE_ARG(R) r) const
  {
    (void)r;
    return operation_state();
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

template <typename R>
struct connect_member<const sender, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct no_bulk_execute
{
};

struct const_member_bulk_execute
{
  const_member_bulk_execute()
  {
  }

  template <typename F>
  sender bulk_execute(BOOST_ASIO_MOVE_ARG(F), std::size_t) const
  {
    ++call_count;
    return sender();
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

template <typename F, typename N>
struct bulk_execute_member<const const_member_bulk_execute, F, N>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef sender result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct free_bulk_execute
{
  free_bulk_execute()
  {
  }

  template <typename F>
  friend sender bulk_execute(const free_bulk_execute&,
      BOOST_ASIO_MOVE_ARG(F), std::size_t)
  {
    ++call_count;
    return sender();
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

template <typename F, typename N>
struct bulk_execute_free<const free_bulk_execute, F, N>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef sender result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

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
    typename boost::asio::decay<F>::type tmp(BOOST_ASIO_MOVE_CAST(F)(f));
    tmp();
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

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)
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

void test_can_bulk_execute()
{
  BOOST_ASIO_CONSTEXPR bool b1 = exec::can_bulk_execute<
      no_bulk_execute&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b1 == false);

  BOOST_ASIO_CONSTEXPR bool b2 = exec::can_bulk_execute<
      const no_bulk_execute&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b2 == false);

  BOOST_ASIO_CONSTEXPR bool b3 = exec::can_bulk_execute<
      const_member_bulk_execute&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b3 == true);

  BOOST_ASIO_CONSTEXPR bool b4 = exec::can_bulk_execute<
      const const_member_bulk_execute&,
      exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b4 == true);

  BOOST_ASIO_CONSTEXPR bool b5 = exec::can_bulk_execute<
      free_bulk_execute&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b5 == true);

  BOOST_ASIO_CONSTEXPR bool b6 = exec::can_bulk_execute<
      const free_bulk_execute&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b6 == true);

  BOOST_ASIO_CONSTEXPR bool b7 = exec::can_bulk_execute<
      executor&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b7 == true);

  BOOST_ASIO_CONSTEXPR bool b8 = exec::can_bulk_execute<
      const executor&, exec::invocable_archetype, std::size_t>::value;
  BOOST_ASIO_CHECK(b8 == true);
}

void handler(std::size_t)
{
}

void counting_handler(std::size_t)
{
  ++call_count;
}

void completion_handler()
{
  ++call_count;
}

void test_bulk_execute()
{
  call_count = 0;
  const_member_bulk_execute ex1;
  exec::bulk_execute(ex1, handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const const_member_bulk_execute ex2;
  exec::bulk_execute(ex2, handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::bulk_execute(const_member_bulk_execute(), handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  free_bulk_execute ex3;
  exec::bulk_execute(ex3, handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const free_bulk_execute ex4;
  exec::bulk_execute(ex4, handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::bulk_execute(free_bulk_execute(), handler, 2);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  executor ex5;
  exec::execute(
      exec::bulk_execute(ex5, counting_handler, 10u),
      completion_handler);
  BOOST_ASIO_CHECK(call_count == 11);

  call_count = 0;
  const executor ex6;
  exec::execute(
      exec::bulk_execute(ex6, counting_handler, 10u),
      completion_handler);
  BOOST_ASIO_CHECK(call_count == 11);

  call_count = 0;
  exec::execute(
      exec::bulk_execute(executor(), counting_handler, 10u),
      completion_handler);
  BOOST_ASIO_CHECK(call_count == 11);
}

BOOST_ASIO_TEST_SUITE
(
  "bulk_execute",
  BOOST_ASIO_TEST_CASE(test_can_bulk_execute)
  BOOST_ASIO_TEST_CASE(test_bulk_execute)
)
