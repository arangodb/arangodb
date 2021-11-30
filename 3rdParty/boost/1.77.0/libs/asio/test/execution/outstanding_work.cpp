//
// outstanding_work.cpp
// ~~~~~~~~~~~~~~~~~~~~
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
#include <boost/asio/execution/outstanding_work.hpp>

#include <boost/asio/prefer.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/require.hpp>
#include "../unit_test.hpp"

namespace exec = boost::asio::execution;

typedef exec::outstanding_work_t s;
typedef exec::outstanding_work_t::untracked_t n1;
typedef exec::outstanding_work_t::tracked_t n2;

struct ex_nq_nr
{
  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_nq_nr&, const ex_nq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_nq_nr&, const ex_nq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <>
struct is_executor<ex_nq_nr> : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

template <typename ResultType, typename ParamType, typename Result>
struct ex_cq_nr
{
  static BOOST_ASIO_CONSTEXPR ResultType query(ParamType) BOOST_ASIO_NOEXCEPT
  {
    return Result();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_cq_nr&, const ex_cq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_cq_nr&, const ex_cq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <typename ResultType, typename ParamType, typename Result>
struct is_executor<ex_cq_nr<ResultType, ParamType, Result> >
  : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

template <typename ResultType, typename ParamType,
  typename Result, typename Param>
struct query_static_constexpr_member<
  ex_cq_nr<ResultType, ParamType, Result>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, ParamType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef Result result_type; // Must return raw result type.

  static BOOST_ASIO_CONSTEXPR result_type value()
  {
    return Result();
  }
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

template <typename ResultType, typename ParamType, typename Result>
struct ex_mq_nr
{
  ResultType query(ParamType) const BOOST_ASIO_NOEXCEPT
  {
    return Result();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_mq_nr&, const ex_mq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_mq_nr&, const ex_mq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <typename ResultType, typename ParamType, typename Result>
struct is_executor<ex_mq_nr<ResultType, ParamType, Result> >
  : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

template <typename ResultType, typename ParamType,
  typename Result, typename Param>
struct query_member<
  ex_mq_nr<ResultType, ParamType, Result>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, ParamType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ResultType result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

template <typename ResultType, typename ParamType, typename Result>
struct ex_fq_nr
{
  friend ResultType query(const ex_fq_nr&, ParamType) BOOST_ASIO_NOEXCEPT
  {
    return Result();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_fq_nr&, const ex_fq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_fq_nr&, const ex_fq_nr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <typename ResultType, typename ParamType, typename Result>
struct is_executor<ex_fq_nr<ResultType, ParamType, Result> >
  : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_FREE_TRAIT)

template <typename ResultType, typename ParamType,
  typename Result, typename Param>
struct query_free<
  ex_fq_nr<ResultType, ParamType, Result>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, ParamType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ResultType result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_FREE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

template <typename CurrentType, typename OtherType>
struct ex_mq_mr
{
  CurrentType query(CurrentType) const BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  CurrentType query(OtherType) const BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  ex_mq_mr<CurrentType, OtherType> require(
      CurrentType) const BOOST_ASIO_NOEXCEPT
  {
    return ex_mq_mr<CurrentType, OtherType>();
  }

  ex_mq_mr<OtherType, CurrentType> require(
      OtherType) const BOOST_ASIO_NOEXCEPT
  {
    return ex_mq_mr<OtherType, CurrentType>();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_mq_mr&, const ex_mq_mr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_mq_mr&, const ex_mq_mr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

template <typename CurrentType>
struct ex_mq_mr<CurrentType, CurrentType>
{
  CurrentType query(CurrentType) const BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  ex_mq_mr<CurrentType, CurrentType> require(
      CurrentType) const BOOST_ASIO_NOEXCEPT
  {
    return ex_mq_mr<CurrentType, CurrentType>();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_mq_mr&, const ex_mq_mr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_mq_mr&, const ex_mq_mr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <typename CurrentType, typename OtherType>
struct is_executor<ex_mq_mr<CurrentType, OtherType> >
  : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

template <typename CurrentType, typename OtherType, typename Param>
struct query_member<
  ex_mq_mr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, CurrentType>::value
      || boost::asio::is_convertible<Param, OtherType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef CurrentType result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

template <typename CurrentType, typename OtherType, typename Param>
struct require_member<
  ex_mq_mr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, CurrentType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_mq_mr<CurrentType, OtherType> result_type;
};

template <typename CurrentType, typename OtherType, typename Param>
struct require_member<
  ex_mq_mr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, OtherType>::value
      && !boost::asio::is_same<CurrentType, OtherType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_mq_mr<OtherType, CurrentType> result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

template <typename CurrentType, typename OtherType>
struct ex_fq_fr
{
  friend CurrentType query(const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  friend CurrentType query(const ex_fq_fr&, OtherType) BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  friend ex_fq_fr<CurrentType, OtherType> require(
      const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<CurrentType, OtherType>();
  }

  friend ex_fq_fr<OtherType, CurrentType> require(
      const ex_fq_fr&, OtherType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<OtherType, CurrentType>();
  }

  friend ex_fq_fr<CurrentType, OtherType> prefer(
      const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<CurrentType, OtherType>();
  }

  friend ex_fq_fr<OtherType, CurrentType> prefer(
      const ex_fq_fr&, OtherType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<OtherType, CurrentType>();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_fq_fr&, const ex_fq_fr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_fq_fr&, const ex_fq_fr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

template <typename CurrentType>
struct ex_fq_fr<CurrentType, CurrentType>
{
  friend CurrentType query(const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return CurrentType();
  }

  friend ex_fq_fr<CurrentType, CurrentType> require(
      const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<CurrentType, CurrentType>();
  }

  friend ex_fq_fr<CurrentType, CurrentType> prefer(
      const ex_fq_fr&, CurrentType) BOOST_ASIO_NOEXCEPT
  {
    return ex_fq_fr<CurrentType, CurrentType>();
  }

  template <typename F>
  void execute(const F&) const
  {
  }

  friend bool operator==(const ex_fq_fr&, const ex_fq_fr&) BOOST_ASIO_NOEXCEPT
  {
    return true;
  }

  friend bool operator!=(const ex_fq_fr&, const ex_fq_fr&) BOOST_ASIO_NOEXCEPT
  {
    return false;
  }
};

#if !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace execution {

template <typename CurrentType, typename OtherType>
struct is_executor<ex_fq_fr<CurrentType, OtherType> >
  : boost::asio::true_type
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT)

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_FREE_TRAIT)

template <typename CurrentType, typename OtherType, typename Param>
struct query_free<
  ex_fq_fr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, CurrentType>::value
      || boost::asio::is_convertible<Param, OtherType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef CurrentType result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_FREE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_FREE_TRAIT)

template <typename CurrentType, typename OtherType, typename Param>
struct require_free<
  ex_fq_fr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, CurrentType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_fq_fr<CurrentType, OtherType> result_type;
};

template <typename CurrentType, typename OtherType, typename Param>
struct require_free<
  ex_fq_fr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, OtherType>::value
      && !boost::asio::is_same<CurrentType, OtherType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_fq_fr<OtherType, CurrentType> result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_FREE_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_PREFER_FREE_TRAIT)

template <typename CurrentType, typename OtherType, typename Param>
struct prefer_free<
  ex_fq_fr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, CurrentType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_fq_fr<CurrentType, OtherType> result_type;
};

template <typename CurrentType, typename OtherType, typename Param>
struct prefer_free<
  ex_fq_fr<CurrentType, OtherType>, Param,
  typename boost::asio::enable_if<
    boost::asio::is_convertible<Param, OtherType>::value
      && !boost::asio::is_same<CurrentType, OtherType>::value
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);

  typedef ex_fq_fr<OtherType, CurrentType> result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_PREFER_FREE_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

template <typename Executor, typename Param, bool ExpectedResult>
void test_can_query()
{
  BOOST_ASIO_CONSTEXPR bool b1 =
    boost::asio::can_query<Executor, Param>::value;
  BOOST_ASIO_CHECK(b1 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b2 =
    boost::asio::can_query<const Executor, Param>::value;
  BOOST_ASIO_CHECK(b2 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b3 =
    boost::asio::can_query<Executor&, Param>::value;
  BOOST_ASIO_CHECK(b3 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b4 =
    boost::asio::can_query<const Executor&, Param>::value;
  BOOST_ASIO_CHECK(b4 == ExpectedResult);
}

template <typename Executor, typename Param, typename ExpectedResult>
void test_query()
{
  exec::outstanding_work_t result1 = boost::asio::query(Executor(), Param());
  BOOST_ASIO_CHECK(result1 == ExpectedResult());

  Executor ex1 = {};
  exec::outstanding_work_t result2 = boost::asio::query(ex1, Param());
  BOOST_ASIO_CHECK(result2 == ExpectedResult());

  const Executor ex2 = {};
  exec::outstanding_work_t result3 = boost::asio::query(ex2, Param());
  BOOST_ASIO_CHECK(result3 == ExpectedResult());
}

template <typename Executor, typename Param, typename ExpectedResult>
void test_constexpr_query()
{
#if defined(BOOST_ASIO_HAS_CONSTEXPR)
  constexpr Executor ex1 = {};
  constexpr exec::outstanding_work_t result1 = boost::asio::query(ex1, Param());
  BOOST_ASIO_CHECK(result1 == ExpectedResult());
#endif // defined(BOOST_ASIO_HAS_CONSTEXPR)
}

template <typename Executor, typename Param, bool ExpectedResult>
void test_can_require()
{
  BOOST_ASIO_CONSTEXPR bool b1 =
    boost::asio::can_require<Executor, Param>::value;
  BOOST_ASIO_CHECK(b1 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b2 =
    boost::asio::can_require<const Executor, Param>::value;
  BOOST_ASIO_CHECK(b2 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b3 =
    boost::asio::can_require<Executor&, Param>::value;
  BOOST_ASIO_CHECK(b3 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b4 =
    boost::asio::can_require<const Executor&, Param>::value;
  BOOST_ASIO_CHECK(b4 == ExpectedResult);
}

template <typename Executor, typename Param, typename ExpectedResult>
void test_require()
{
  BOOST_ASIO_CHECK(
      boost::asio::query(
        boost::asio::require(Executor(), Param()),
        Param()) == ExpectedResult());

  Executor ex1 = {};
  BOOST_ASIO_CHECK(
      boost::asio::query(
        boost::asio::require(ex1, Param()),
        Param()) == ExpectedResult());

  const Executor ex2 = {};
  BOOST_ASIO_CHECK(
      boost::asio::query(
        boost::asio::require(ex2, Param()),
        Param()) == ExpectedResult());
}

template <typename Executor, typename Param, bool ExpectedResult>
void test_can_prefer()
{
  BOOST_ASIO_CONSTEXPR bool b1 =
    boost::asio::can_prefer<Executor, Param>::value;
  BOOST_ASIO_CHECK(b1 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b2 =
    boost::asio::can_prefer<const Executor, Param>::value;
  BOOST_ASIO_CHECK(b2 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b3 =
    boost::asio::can_prefer<Executor&, Param>::value;
  BOOST_ASIO_CHECK(b3 == ExpectedResult);

  BOOST_ASIO_CONSTEXPR bool b4 =
    boost::asio::can_prefer<const Executor&, Param>::value;
  BOOST_ASIO_CHECK(b4 == ExpectedResult);
}

template <typename Executor, typename Param, typename ExpectedResult>
void test_prefer()
{
  BOOST_ASIO_CHECK(
      s(boost::asio::query(
        boost::asio::prefer(Executor(), Param()),
          s())) == s(ExpectedResult()));

  Executor ex1 = {};
  BOOST_ASIO_CHECK(
      s(boost::asio::query(
        boost::asio::prefer(ex1, Param()),
          s())) == s(ExpectedResult()));

  const Executor ex2 = {};
  BOOST_ASIO_CHECK(
      s(boost::asio::query(
        boost::asio::prefer(ex2, Param()),
          s())) == s(ExpectedResult()));
}

void test_vars()
{
  BOOST_ASIO_CHECK(s() == exec::outstanding_work);
  BOOST_ASIO_CHECK(n1() == exec::outstanding_work.untracked);
  BOOST_ASIO_CHECK(n2() == exec::outstanding_work.tracked);
}

BOOST_ASIO_TEST_SUITE
(
  "outstanding_work",

  BOOST_ASIO_TEST_CASE3(test_can_query<ex_nq_nr, s, true>)
  BOOST_ASIO_TEST_CASE3(test_can_query<ex_nq_nr, n1, true>)
  BOOST_ASIO_TEST_CASE3(test_can_query<ex_nq_nr, n2, false>)

  BOOST_ASIO_TEST_CASE3(test_query<ex_nq_nr, s, n1>)
  BOOST_ASIO_TEST_CASE3(test_query<ex_nq_nr, n1, n1>)

  BOOST_ASIO_TEST_CASE3(test_constexpr_query<ex_nq_nr, s, n1>)
  BOOST_ASIO_TEST_CASE3(test_constexpr_query<ex_nq_nr, n1, n1>)

  BOOST_ASIO_TEST_CASE3(test_can_require<ex_nq_nr, s, false>)
  BOOST_ASIO_TEST_CASE3(test_can_require<ex_nq_nr, n1, true>)
  BOOST_ASIO_TEST_CASE3(test_can_require<ex_nq_nr, n2, false>)

  BOOST_ASIO_TEST_CASE3(test_require<ex_nq_nr, n1, n1>)

  BOOST_ASIO_TEST_CASE3(test_can_prefer<ex_nq_nr, s, false>)
  BOOST_ASIO_TEST_CASE3(test_can_prefer<ex_nq_nr, n1, true>)
  BOOST_ASIO_TEST_CASE3(test_can_prefer<ex_nq_nr, n2, true>)

  BOOST_ASIO_TEST_CASE3(test_prefer<ex_nq_nr, n1, n1>)
  BOOST_ASIO_TEST_CASE3(test_prefer<ex_nq_nr, n2, n1>)

  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n1, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n2, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_cq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n1, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n2, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n1, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n2, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_cq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n1, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n2, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n1, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n2, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_constexpr_query<ex_cq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n1, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n2, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_cq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_require<ex_cq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_cq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n1, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n2, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_cq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n1, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n2, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_mq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n1, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n2, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n1, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n2, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_mq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, s, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<s, n2, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n1, s, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n1, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n2, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_mq_nr<n2, s, n2>, n2, false>)

  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_mq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n1, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n2, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_mq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n1, s, n1>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n2, s, n2>, s, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_query<ex_fq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n1, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n2, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n1, s, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n2, s, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_query<ex_fq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, s, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n1, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<s, n2, n2>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n1, s, n1>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n1, s, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n2, s, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE5(test_can_require<ex_fq_nr<n2, s, n2>, n2, false>)

  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, s, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<s, n2, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n1, s, n1>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n1, s, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n1, s, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n2, s, n2>, s, false>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n2, s, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE5(test_can_prefer<ex_fq_nr<n2, s, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, s, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n1, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n1, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n2, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n2, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<s, n2, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<n1, s, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<n1, s, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<n2, s, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE5(test_prefer<ex_fq_nr<n2, s, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n1>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n2>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n1>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n2>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_mq_mr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n1, n2>, s, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n2, n1>, s, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_mq_mr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_mq_mr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_mq_mr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_mq_mr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n1, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n2, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_mq_mr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n1>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n2>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n1>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n2>, s, true>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE4(test_can_query<ex_fq_fr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n1, n1>, s, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n1, n2>, s, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n2, n1>, s, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n2, n2>, s, n2>)
  BOOST_ASIO_TEST_CASE4(test_query<ex_fq_fr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n1>, n2, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n2>, n1, false>)
  BOOST_ASIO_TEST_CASE4(test_can_require<ex_fq_fr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_require<ex_fq_fr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n1, n2>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n1>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n1>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n1>, n2, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n2>, s, false>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n2>, n1, true>)
  BOOST_ASIO_TEST_CASE4(test_can_prefer<ex_fq_fr<n2, n2>, n2, true>)

  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n1, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n1, n1>, n2, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n1, n2>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n1, n2>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n2, n1>, n1, n1>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n2, n1>, n2, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n2, n2>, n1, n2>)
  BOOST_ASIO_TEST_CASE4(test_prefer<ex_fq_fr<n2, n2>, n2, n2>)

  BOOST_ASIO_TEST_CASE(test_vars)
)
