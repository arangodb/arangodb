// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if (BOOST_PARAMETER_MAX_ARITY < 10)
#error Define BOOST_PARAMETER_MAX_ARITY as 10 or greater.
#endif

#include <boost/parameter.hpp>

namespace test {

    BOOST_PARAMETER_NAME(in(lrc0))
    BOOST_PARAMETER_NAME(out(lr0))
    BOOST_PARAMETER_NAME(in(rrc0))
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    BOOST_PARAMETER_NAME(consume(rr0))
#else
    BOOST_PARAMETER_NAME(rr0)
#endif
    BOOST_PARAMETER_NAME(in(lrc1))
    BOOST_PARAMETER_NAME(out(lr1))
    BOOST_PARAMETER_NAME(in(rrc1))
    BOOST_PARAMETER_NAME(in(lrc2))
    BOOST_PARAMETER_NAME(out(lr2))
    BOOST_PARAMETER_NAME(rr2)

    struct g_parameters
      : boost::parameter::parameters<
            boost::parameter::required<test::tag::lrc0>
          , boost::parameter::required<test::tag::lr0>
          , boost::parameter::required<test::tag::rrc0>
          , boost::parameter::required<test::tag::rr0>
          , boost::parameter::required<test::tag::lrc1>
          , boost::parameter::required<test::tag::lr1>
          , boost::parameter::required<test::tag::rrc1>
          , boost::parameter::optional<test::tag::lrc2>
          , boost::parameter::optional<test::tag::lr2>
          , boost::parameter::optional<test::tag::rr2>
        >
    {
    };
} // namespace test

#include <boost/parameter/macros.hpp>
#include <boost/core/lightweight_test.hpp>
#include "evaluate_category.hpp"

namespace test {

    struct C
    {
        BOOST_PARAMETER_MEMFUN(static int, evaluate, 7, 10, g_parameters)
        {
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<0>(p[test::_lrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , U::evaluate_category<0>(p[test::_lr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<1>(p[test::_lrc1])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , U::evaluate_category<1>(p[test::_lr1])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<2>(
                    p[test::_lrc2 | test::lvalue_const_bitset<2>()]
                )
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , U::evaluate_category<2>(
                    p[test::_lr2 || test::lvalue_bitset_function<2>()]
                )
            );
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , U::evaluate_category<0>(p[test::_rrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , U::evaluate_category<0>(p[test::_rr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , U::evaluate_category<1>(p[test::_rrc1])
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , U::evaluate_category<2>(
                    p[test::_rr2 || test::rvalue_bitset_function<2>()]
                )
            );
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<0>(p[test::_rrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<0>(p[test::_rr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<1>(p[test::_rrc1])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , U::evaluate_category<2>(
                    p[test::_rr2 || test::rvalue_bitset_function<2>()]
                )
            );
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING

            return 0;
        }
    };
} // namespace test

#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
#include <boost/core/ref.hpp>
#endif

int main()
{
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) || \
    (10 < BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY)
    test::C::evaluate(
        test::lvalue_const_bitset<0>()
      , test::lvalue_bitset<0>()
      , test::rvalue_const_bitset<0>()
      , test::rvalue_bitset<0>()
      , test::lvalue_const_bitset<1>()
      , test::lvalue_bitset<1>()
      , test::rvalue_const_bitset<1>()
    );
    test::C::evaluate(
        test::lvalue_const_bitset<0>()
      , test::lvalue_bitset<0>()
      , test::rvalue_const_bitset<0>()
      , test::rvalue_bitset<0>()
      , test::lvalue_const_bitset<1>()
      , test::lvalue_bitset<1>()
      , test::rvalue_const_bitset<1>()
      , test::lvalue_const_bitset<2>()
      , test::lvalue_bitset<2>()
      , test::rvalue_bitset<2>()
    );
#else   // no perfect forwarding support and no exponential overloads
    test::C::evaluate(
        test::lvalue_const_bitset<0>()
      , boost::ref(test::lvalue_bitset<0>())
      , test::rvalue_const_bitset<0>()
      , test::rvalue_bitset<0>()
      , test::lvalue_const_bitset<1>()
      , boost::ref(test::lvalue_bitset<1>())
      , test::rvalue_const_bitset<1>()
    );
    test::C::evaluate(
        test::lvalue_const_bitset<0>()
      , boost::ref(test::lvalue_bitset<0>())
      , test::rvalue_const_bitset<0>()
      , test::rvalue_bitset<0>()
      , test::lvalue_const_bitset<1>()
      , boost::ref(test::lvalue_bitset<1>())
      , test::rvalue_const_bitset<1>()
      , test::lvalue_const_bitset<2>()
      , boost::ref(test::lvalue_bitset<2>())
      , test::rvalue_bitset<2>()
    );
#endif  // perfect forwarding support, or exponential overloads
    return boost::report_errors();
}

