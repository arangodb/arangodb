//
// receiver.cpp
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
#include <boost/asio/execution/receiver.hpp>

#include <string>
#include <boost/system/error_code.hpp>
#include "../unit_test.hpp"

struct not_a_receiver
{
};

struct receiver
{
  receiver()
  {
  }

  receiver(const receiver&)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver(receiver&&)
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

struct receiver_of_0
{
  receiver_of_0()
  {
  }

  receiver_of_0(const receiver_of_0&)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver_of_0(receiver_of_0&&)
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

  void set_value()
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

template <typename E>
struct set_error_member<receiver_of_0, E>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_done_member<receiver_of_0>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_value_member<receiver_of_0, void()>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct receiver_of_1
{
  receiver_of_1()
  {
  }

  receiver_of_1(const receiver_of_1&)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver_of_1(receiver_of_1&&)
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

  void set_value(int) BOOST_ASIO_NOEXCEPT
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

template <typename E>
struct set_error_member<receiver_of_1, E>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_done_member<receiver_of_1>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_value_member<receiver_of_1, void(int)>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct receiver_of_2
{
  receiver_of_2()
  {
  }

  receiver_of_2(const receiver_of_2&)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver_of_2(receiver_of_2&&)
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

  void set_value(int, std::string)
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

template <typename E>
struct set_error_member<receiver_of_2, E>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_done_member<receiver_of_2>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_value_member<receiver_of_2, void(int, std::string)>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void is_receiver_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver<
        void,
        boost::system::error_code
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver<
        not_a_receiver,
        boost::system::error_code
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver<
        receiver,
        boost::system::error_code
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver<
        receiver_of_0,
        boost::system::error_code
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver<
        receiver_of_1,
        boost::system::error_code
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver<
        receiver_of_2,
        boost::system::error_code
      >::value));
}

void is_receiver_of_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        void
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        void,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        not_a_receiver
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        not_a_receiver,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        not_a_receiver,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver_of<
        receiver_of_0
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_0,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_0,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_1
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver_of<
        receiver_of_1,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_1,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_2
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_receiver_of<
        receiver_of_2,
        int
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_receiver_of<
        receiver_of_2,
        int,
        std::string
      >::value));
}

void is_nothrow_receiver_of_test()
{
  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        void
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        void,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        not_a_receiver
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        not_a_receiver,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        not_a_receiver,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_0
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_0,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_0,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_1
      >::value));

  BOOST_ASIO_CHECK((
      boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_1,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_1,
        int,
        std::string
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_2
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_2,
        int
      >::value));

  BOOST_ASIO_CHECK((
      !boost::asio::execution::is_nothrow_receiver_of<
        receiver_of_2,
        int,
        std::string
      >::value));
}

BOOST_ASIO_TEST_SUITE
(
  "receiver",
  BOOST_ASIO_TEST_CASE(is_receiver_test)
  BOOST_ASIO_TEST_CASE(is_receiver_of_test)
  BOOST_ASIO_TEST_CASE(is_nothrow_receiver_of_test)
)
