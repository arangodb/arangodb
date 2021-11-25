//
// prefer_only.cpp
// ~~~~~~~~~~~~~~~
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
#include <boost/asio/execution/prefer_only.hpp>

#include <boost/asio/execution/any_executor.hpp>
#include "../unit_test.hpp"

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

using namespace boost::asio;

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = std;
#endif

static int possibly_blocking_count = 0;
static int never_blocking_count = 0;

struct possibly_blocking_executor
{
  template <typename F>
  void execute(const F&) const
  {
    ++possibly_blocking_count;
  }

  friend bool operator==(const possibly_blocking_executor&,
      const possibly_blocking_executor&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const possibly_blocking_executor&,
      const possibly_blocking_executor&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename F>
struct execute_member<possibly_blocking_executor, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <>
struct equality_comparable<possibly_blocking_executor>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct never_blocking_executor
{
  static BOOST_ASIO_CONSTEXPR execution::blocking_t::never_t
    query(execution::blocking_t) BOOST_ASIO_NOEXCEPT
  {
    return execution::blocking_t::never_t();
  }

  template <typename F>
  void execute(const F&) const
  {
    ++never_blocking_count;
  }

  friend bool operator==(const never_blocking_executor&,
      const never_blocking_executor&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const never_blocking_executor&,
      const never_blocking_executor&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename F>
struct execute_member<never_blocking_executor, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <>
struct equality_comparable<never_blocking_executor>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

template <typename Param>
struct query_static_constexpr_member<
  never_blocking_executor, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, execution::blocking_t>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef execution::blocking_t::never_t result_type;

  static BOOST_ASIO_CONSTEXPR result_type value()
  {
    return result_type();
  }
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

struct either_blocking_executor
{
  execution::blocking_t blocking_;

  explicit either_blocking_executor(execution::blocking_t b)
    : blocking_(b)
  {
  }

  execution::blocking_t query(execution::blocking_t) const BOOST_ASIO_NOEXCEPT
  {
    return blocking_;
  }

  either_blocking_executor require(execution::blocking_t::possibly_t) const
  {
    return either_blocking_executor(execution::blocking.possibly);
  }

  either_blocking_executor require(execution::blocking_t::never_t) const
  {
    return either_blocking_executor(execution::blocking.never);
  }

  template <typename F>
  void execute(const F&) const
  {
    if (blocking_ == execution::blocking.never)
      ++never_blocking_count;
    else
      ++possibly_blocking_count;
  }

  friend bool operator==(const either_blocking_executor& a,
      const either_blocking_executor& b) BOOST_ASIO_NOEXCEPT
  {
    return a.blocking_ == b.blocking_;
  }

  friend bool operator!=(const either_blocking_executor& a,
      const either_blocking_executor& b) BOOST_ASIO_NOEXCEPT
  {
    return a.blocking_ != b.blocking_;
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename F>
struct execute_member<either_blocking_executor, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <>
struct equality_comparable<either_blocking_executor>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

template <typename Param>
struct query_member<
  either_blocking_executor, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, execution::blocking_t>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef execution::blocking_t result_type;
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

template <typename Param>
struct require_member<
  either_blocking_executor, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, execution::blocking_t>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);

  typedef either_blocking_executor result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void prefer_only_executor_query_test()
{
  typedef execution::any_executor<
      execution::blocking_t,
      execution::prefer_only<execution::blocking_t::possibly_t>,
      execution::prefer_only<execution::blocking_t::never_t>
    > executor_type;

  executor_type ex1 = possibly_blocking_executor();

  BOOST_ASIO_CHECK(
      boost::asio::query(ex1, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex1, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex1, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex2 = boost::asio::prefer(ex1, execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex2, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex2, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex2, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex3 = boost::asio::prefer(ex1, execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex3, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex3, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex3, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex4 = never_blocking_executor();

  BOOST_ASIO_CHECK(
      boost::asio::query(ex4, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex4, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex4, execution::blocking.never)
        == execution::blocking.never);

  executor_type ex5 = boost::asio::prefer(ex4, execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex5, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex5, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex5, execution::blocking.never)
        == execution::blocking.never);

  executor_type ex6 = boost::asio::prefer(ex4, execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex6, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex6, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex6, execution::blocking.never)
        == execution::blocking.never);

  executor_type ex7 = either_blocking_executor(execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex7, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex7, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex7, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex8 = boost::asio::prefer(ex7, execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex8, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex8, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex8, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex9 = boost::asio::prefer(ex7, execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex9, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex9, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex9, execution::blocking.never)
        == execution::blocking.never);

  executor_type ex10 = either_blocking_executor(execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex10, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex10, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex10, execution::blocking.never)
        == execution::blocking.never);

  executor_type ex11 = boost::asio::prefer(ex7, execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex11, execution::blocking)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex11, execution::blocking.possibly)
        == execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex11, execution::blocking.never)
        == execution::blocking.possibly);

  executor_type ex12 = boost::asio::prefer(ex7, execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex12, execution::blocking)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex12, execution::blocking.possibly)
        == execution::blocking.never);

  BOOST_ASIO_CHECK(
      boost::asio::query(ex12, execution::blocking.never)
        == execution::blocking.never);
}

void do_nothing()
{
}

void prefer_only_executor_execute_test()
{
  typedef execution::any_executor<
      execution::blocking_t,
      execution::prefer_only<execution::blocking_t::possibly_t>,
      execution::prefer_only<execution::blocking_t::never_t>
    > executor_type;

  executor_type ex1 = possibly_blocking_executor();

  execution::execute(ex1, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 1);
  BOOST_ASIO_CHECK(never_blocking_count == 0);

  executor_type ex2 = boost::asio::prefer(ex1, execution::blocking.possibly);

  execution::execute(ex2, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 2);
  BOOST_ASIO_CHECK(never_blocking_count == 0);

  executor_type ex3 = boost::asio::prefer(ex1, execution::blocking.never);

  execution::execute(ex3, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 3);
  BOOST_ASIO_CHECK(never_blocking_count == 0);

  executor_type ex4 = never_blocking_executor();

  execution::execute(ex4, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 3);
  BOOST_ASIO_CHECK(never_blocking_count == 1);

  executor_type ex5 = boost::asio::prefer(ex4, execution::blocking.possibly);

  execution::execute(ex5, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 3);
  BOOST_ASIO_CHECK(never_blocking_count == 2);

  executor_type ex6 = boost::asio::prefer(ex4, execution::blocking.never);

  execution::execute(ex6, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 3);
  BOOST_ASIO_CHECK(never_blocking_count == 3);

  executor_type ex7 = either_blocking_executor(execution::blocking.possibly);

  execution::execute(ex7, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 4);
  BOOST_ASIO_CHECK(never_blocking_count == 3);

  executor_type ex8 = boost::asio::prefer(ex7, execution::blocking.possibly);

  execution::execute(ex8, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 5);
  BOOST_ASIO_CHECK(never_blocking_count == 3);

  executor_type ex9 = boost::asio::prefer(ex7, execution::blocking.never);

  execution::execute(ex9, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 5);
  BOOST_ASIO_CHECK(never_blocking_count == 4);

  executor_type ex10 = either_blocking_executor(execution::blocking.never);

  execution::execute(ex10, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 5);
  BOOST_ASIO_CHECK(never_blocking_count == 5);

  executor_type ex11 = boost::asio::prefer(ex7, execution::blocking.possibly);

  execution::execute(ex11, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 6);
  BOOST_ASIO_CHECK(never_blocking_count == 5);

  executor_type ex12 = boost::asio::prefer(ex7, execution::blocking.never);

  execution::execute(ex12, &do_nothing);
  BOOST_ASIO_CHECK(possibly_blocking_count == 6);
  BOOST_ASIO_CHECK(never_blocking_count == 6);
}

BOOST_ASIO_TEST_SUITE
(
  "prefer_only",
  BOOST_ASIO_TEST_CASE(prefer_only_executor_query_test)
  BOOST_ASIO_TEST_CASE(prefer_only_executor_execute_test)
)
