//
// sender.cpp
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
#include <boost/asio/execution/sender.hpp>

#include <boost/system/error_code.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

struct not_a_sender
{
};

struct sender_using_base :
  boost::asio::execution::sender_base
{
  sender_using_base()
  {
  }
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

#if defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)

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

struct typed_sender
{
  template <
      template <typename...> class Tuple,
      template <typename...> class Variant>
  using value_types = Variant<Tuple<int>>;

  template <template <typename...> class Variant>
  using error_types = Variant<boost::system::error_code>;

  BOOST_ASIO_STATIC_CONSTEXPR(bool, sends_done = true);

  typed_sender()
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
struct connect_member<const typed_sender, R>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef operation_state result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

#endif // defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)

template <typename T>
bool is_unspecialised(T*, ...)
{
  return false;
}

template <typename T>
bool is_unspecialised(T*,
    typename boost::asio::void_type<
      typename exec::sender_traits<
        T>::asio_execution_sender_traits_base_is_unspecialised
    >::type*)
{
  return true;
}

void test_sender_traits()
{
  not_a_sender s1;
  BOOST_ASIO_CHECK(is_unspecialised(&s1, static_cast<void*>(0)));

  sender_using_base s2;
  BOOST_ASIO_CHECK(!is_unspecialised(&s2, static_cast<void*>(0)));

  executor s3;
  BOOST_ASIO_CHECK(!is_unspecialised(&s3, static_cast<void*>(0)));

#if defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
  typed_sender s4;
  BOOST_ASIO_CHECK(!is_unspecialised(&s4, static_cast<void*>(0)));
#endif // defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
}

void test_is_sender()
{
  BOOST_ASIO_CHECK(!exec::is_sender<void>::value);
  BOOST_ASIO_CHECK(!exec::is_sender<not_a_sender>::value);
  BOOST_ASIO_CHECK(exec::is_sender<sender_using_base>::value);
  BOOST_ASIO_CHECK(exec::is_sender<executor>::value);

#if defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
  BOOST_ASIO_CHECK(exec::is_sender<typed_sender>::value);
#endif // defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
}

void test_is_typed_sender()
{
  BOOST_ASIO_CHECK(!exec::is_typed_sender<void>::value);
  BOOST_ASIO_CHECK(!exec::is_typed_sender<not_a_sender>::value);
  BOOST_ASIO_CHECK(!exec::is_typed_sender<sender_using_base>::value);

#if defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
  BOOST_ASIO_CHECK(exec::is_typed_sender<executor>::value);
  BOOST_ASIO_CHECK(exec::is_typed_sender<typed_sender>::value);
#endif // defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_TYPED_SENDER_TRAIT)
}

BOOST_ASIO_TEST_SUITE
(
  "sender",
  BOOST_ASIO_TEST_CASE(test_sender_traits)
  BOOST_ASIO_TEST_CASE(test_is_sender)
  BOOST_ASIO_TEST_CASE(test_is_typed_sender)
)
