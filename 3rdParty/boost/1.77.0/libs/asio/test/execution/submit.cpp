//
// submit.cpp
// ~~~~~~~~~~
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
#include <boost/asio/execution/submit.hpp>

#include <boost/system/error_code.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

static int call_count = 0;

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

struct no_submit_1
{
};

struct no_submit_2 : exec::sender_base
{
};

struct no_submit_3
{
  template <typename R>
  void submit(BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

namespace boost {
namespace asio {
namespace traits {

template <typename R>
struct submit_member<no_submit_3, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

struct const_member_submit : exec::sender_base
{
  const_member_submit()
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
    (void)r;
    ++call_count;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

template <typename R>
struct connect_member<const const_member_submit, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

template <typename R>
struct submit_member<const const_member_submit, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct free_submit_const_receiver : exec::sender_base
{
  free_submit_const_receiver()
  {
  }

  template <typename R>
  friend operation_state connect(
      const free_submit_const_receiver&, BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    return operation_state();
  }

  template <typename R>
  friend void submit(
      const free_submit_const_receiver&, BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    ++call_count;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_FREE_TRAIT)

template <typename R>
struct connect_free<const free_submit_const_receiver, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_FREE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

template <typename R>
struct submit_free<const free_submit_const_receiver, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct non_const_member_submit : exec::sender_base
{
  non_const_member_submit()
  {
  }

  template <typename R>
  operation_state connect(BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    return operation_state();
  }

  template <typename R>
  void submit(BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    ++call_count;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

template <typename R>
struct connect_member<non_const_member_submit, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

template <typename R>
struct submit_member<non_const_member_submit, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct free_submit_non_const_receiver : exec::sender_base
{
  free_submit_non_const_receiver()
  {
  }

  template <typename R>
  friend operation_state connect(
      free_submit_non_const_receiver&, BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    return operation_state();
  }

  template <typename R>
  friend void submit(
      free_submit_non_const_receiver&, BOOST_ASIO_MOVE_ARG(R) r)
  {
    (void)r;
    ++call_count;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_FREE_TRAIT)

template <typename R>
struct connect_free<free_submit_non_const_receiver, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_FREE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

template <typename R>
struct submit_free<free_submit_non_const_receiver, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_FREE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct receiver
{
  receiver()
  {
  }

  receiver(const receiver&)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver(receiver&&) BOOST_ASIO_NOEXCEPT
  {
  }
#endif // defined(BOOST_ASIO_HAS_MOVE)

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
    ++call_count;
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

void test_can_submit()
{
  BOOST_ASIO_CONSTEXPR bool b1 = exec::can_submit<
      no_submit_1&, receiver>::value;
  BOOST_ASIO_CHECK(b1 == false);

  BOOST_ASIO_CONSTEXPR bool b2 = exec::can_submit<
      const no_submit_1&, receiver>::value;
  BOOST_ASIO_CHECK(b2 == false);

  BOOST_ASIO_CONSTEXPR bool b3 = exec::can_submit<
      no_submit_2&, receiver>::value;
  BOOST_ASIO_CHECK(b3 == false);

  BOOST_ASIO_CONSTEXPR bool b4 = exec::can_submit<
      const no_submit_2&, receiver>::value;
  BOOST_ASIO_CHECK(b4 == false);

  BOOST_ASIO_CONSTEXPR bool b5 = exec::can_submit<
      no_submit_3&, receiver>::value;
  BOOST_ASIO_CHECK(b5 == false);

  BOOST_ASIO_CONSTEXPR bool b6 = exec::can_submit<
      const no_submit_3&, receiver>::value;
  BOOST_ASIO_CHECK(b6 == false);

  BOOST_ASIO_CONSTEXPR bool b7 = exec::can_submit<
      const_member_submit&, receiver>::value;
  BOOST_ASIO_CHECK(b7 == true);

  BOOST_ASIO_CONSTEXPR bool b8 = exec::can_submit<
      const const_member_submit&, receiver>::value;
  BOOST_ASIO_CHECK(b8 == true);

  BOOST_ASIO_CONSTEXPR bool b9 = exec::can_submit<
      free_submit_const_receiver&, receiver>::value;
  BOOST_ASIO_CHECK(b9 == true);

  BOOST_ASIO_CONSTEXPR bool b10 = exec::can_submit<
      const free_submit_const_receiver&, receiver>::value;
  BOOST_ASIO_CHECK(b10 == true);

  BOOST_ASIO_CONSTEXPR bool b11 = exec::can_submit<
      non_const_member_submit&, receiver>::value;
  BOOST_ASIO_CHECK(b11 == true);

  BOOST_ASIO_CONSTEXPR bool b12 = exec::can_submit<
      const non_const_member_submit&, receiver>::value;
  BOOST_ASIO_CHECK(b12 == false);

  BOOST_ASIO_CONSTEXPR bool b13 = exec::can_submit<
      free_submit_non_const_receiver&, receiver>::value;
  BOOST_ASIO_CHECK(b13 == true);

  BOOST_ASIO_CONSTEXPR bool b14 = exec::can_submit<
      const free_submit_non_const_receiver&, receiver>::value;
  BOOST_ASIO_CHECK(b14 == false);

  BOOST_ASIO_CONSTEXPR bool b15 = exec::can_submit<
      executor&, receiver>::value;
  BOOST_ASIO_CHECK(b15 == true);

  BOOST_ASIO_CONSTEXPR bool b16 = exec::can_submit<
      const executor&, receiver>::value;
  BOOST_ASIO_CHECK(b16 == true);
}

void increment(int* count)
{
  ++(*count);
}

void test_submit()
{
  receiver r;

  call_count = 0;
  const_member_submit s1;
  exec::submit(s1, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const const_member_submit s2;
  exec::submit(s2, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::submit(const_member_submit(), r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  free_submit_const_receiver s3;
  exec::submit(s3, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const free_submit_const_receiver s4;
  exec::submit(s4, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::submit(free_submit_const_receiver(), r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  non_const_member_submit s5;
  exec::submit(s5, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  free_submit_non_const_receiver s6;
  exec::submit(s6, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  executor s7;
  exec::submit(s7, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  const executor s8;
  exec::submit(s8, r);
  BOOST_ASIO_CHECK(call_count == 1);

  call_count = 0;
  exec::submit(executor(), r);
  BOOST_ASIO_CHECK(call_count == 1);
}

BOOST_ASIO_TEST_SUITE
(
  "submit",
  BOOST_ASIO_TEST_CASE(test_can_submit)
  BOOST_ASIO_TEST_CASE(test_submit)
)
