//
// schedule.cpp
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
#include <boost/asio/execution/schedule.hpp>

#include <boost/system/error_code.hpp>
#include <boost/asio/execution/sender.hpp>
#include <boost/asio/execution/submit.hpp>
#include <boost/asio/traits/connect_member.hpp>
#include <boost/asio/traits/start_member.hpp>
#include <boost/asio/traits/submit_member.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

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

  template <typename R>
  void submit(BOOST_ASIO_MOVE_ARG(R) r) const
  {
    typename boost::asio::decay<R>::type tmp(BOOST_ASIO_MOVE_CAST(R)(r));
    exec::set_value(tmp);
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

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

template <typename R>
struct submit_member<const sender, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct no_schedule
{
};

struct const_member_schedule
{
  sender schedule() const BOOST_ASIO_NOEXCEPT
  {
    return sender();
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_MEMBER_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct schedule_member<const const_member_schedule>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef sender result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_MEMBER_TRAIT)

struct free_schedule_const_receiver
{
  friend sender schedule(
      const free_schedule_const_receiver&) BOOST_ASIO_NOEXCEPT
  {
    return sender();
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_FREE_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct schedule_free<const free_schedule_const_receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef sender result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_FREE_TRAIT)

struct non_const_member_schedule
{
  sender schedule() BOOST_ASIO_NOEXCEPT
  {
    return sender();
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_MEMBER_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct schedule_member<non_const_member_schedule>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef sender result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_MEMBER_TRAIT)

struct free_schedule_non_const_receiver
{
  friend sender schedule(
      free_schedule_non_const_receiver&) BOOST_ASIO_NOEXCEPT
  {
    return sender();
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_FREE_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <>
struct schedule_free<free_schedule_non_const_receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef sender result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SCHEDULE_FREE_TRAIT)

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

void test_can_schedule()
{
  BOOST_ASIO_CONSTEXPR bool b1 = exec::can_schedule<
      no_schedule&>::value;
  BOOST_ASIO_CHECK(b1 == false);

  BOOST_ASIO_CONSTEXPR bool b2 = exec::can_schedule<
      const no_schedule&>::value;
  BOOST_ASIO_CHECK(b2 == false);

  BOOST_ASIO_CONSTEXPR bool b3 = exec::can_schedule<
      const_member_schedule&>::value;
  BOOST_ASIO_CHECK(b3 == true);

  BOOST_ASIO_CONSTEXPR bool b4 = exec::can_schedule<
      const const_member_schedule&>::value;
  BOOST_ASIO_CHECK(b4 == true);

  BOOST_ASIO_CONSTEXPR bool b5 = exec::can_schedule<
      free_schedule_const_receiver&>::value;
  BOOST_ASIO_CHECK(b5 == true);

  BOOST_ASIO_CONSTEXPR bool b6 = exec::can_schedule<
      const free_schedule_const_receiver&>::value;
  BOOST_ASIO_CHECK(b6 == true);

  BOOST_ASIO_CONSTEXPR bool b7 = exec::can_schedule<
      non_const_member_schedule&>::value;
  BOOST_ASIO_CHECK(b7 == true);

  BOOST_ASIO_CONSTEXPR bool b8 = exec::can_schedule<
      const non_const_member_schedule&>::value;
  BOOST_ASIO_CHECK(b8 == false);

  BOOST_ASIO_CONSTEXPR bool b9 = exec::can_schedule<
      free_schedule_non_const_receiver&>::value;
  BOOST_ASIO_CHECK(b9 == true);

  BOOST_ASIO_CONSTEXPR bool b10 = exec::can_schedule<
      const free_schedule_non_const_receiver&>::value;
  BOOST_ASIO_CHECK(b10 == false);

  BOOST_ASIO_CONSTEXPR bool b11 = exec::can_schedule<
      executor&>::value;
  BOOST_ASIO_CHECK(b11 == true);

  BOOST_ASIO_CONSTEXPR bool b12 = exec::can_schedule<
      const executor&>::value;
  BOOST_ASIO_CHECK(b12 == true);
}

struct receiver
{
  int* count_;

  receiver(int* count)
    : count_(count)
  {
  }

  receiver(const receiver& other) BOOST_ASIO_NOEXCEPT
    : count_(other.count_)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver(receiver&& other) BOOST_ASIO_NOEXCEPT
    : count_(other.count_)
  {
    other.count_ = 0;
  }
#endif // defined(BOOST_ASIO_HAS_MOVE)

  void set_value() BOOST_ASIO_NOEXCEPT
  {
    ++(*count_);
  }

  template <typename E>
  void set_error(BOOST_ASIO_MOVE_ARG(E) e) BOOST_ASIO_NOEXCEPT
  {
    (void)e;
  }

  void set_done() BOOST_ASIO_NOEXCEPT
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

template <>
struct set_value_member<receiver, void()>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

template <typename E>
struct set_error_member<receiver, E>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_done_member<receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void test_schedule()
{
  int count = 0;
  const_member_schedule ex1 = {};
  exec::submit(
      exec::schedule(ex1),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  const const_member_schedule ex2 = {};
  exec::submit(
      exec::schedule(ex2),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  exec::submit(
      exec::schedule(const_member_schedule()),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  free_schedule_const_receiver ex3 = {};
  exec::submit(
      exec::schedule(ex3),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  const free_schedule_const_receiver ex4 = {};
  exec::submit(
      exec::schedule(ex4),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  exec::submit(
      exec::schedule(free_schedule_const_receiver()),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  non_const_member_schedule ex5 = {};
  exec::submit(
      exec::schedule(ex5),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  free_schedule_non_const_receiver ex6 = {};
  exec::submit(
      exec::schedule(ex6),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  executor ex7;
  exec::submit(
      exec::schedule(ex7),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  const executor ex8;
  exec::submit(
      exec::schedule(ex8),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  exec::submit(
      exec::schedule(executor()),
      receiver(&count));
  BOOST_ASIO_CHECK(count == 1);
}

BOOST_ASIO_TEST_SUITE
(
  "schedule",
  BOOST_ASIO_TEST_CASE(test_can_schedule)
  BOOST_ASIO_TEST_CASE(test_schedule)
)
