// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if ( \
        !defined(__MINGW32__) && \
        defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) \
    ) || defined(BOOST_MSVC)
#if (BOOST_PARAMETER_MAX_ARITY < 8)
#error Define BOOST_PARAMETER_MAX_ARITY as 8 or greater.
#endif
#else   // mingw, or no perfect forwarding support and not msvc
#if (BOOST_PARAMETER_COMPOSE_MAX_ARITY < 8)
#error Define BOOST_PARAMETER_COMPOSE_MAX_ARITY as 8 or greater.
#endif
#endif  // msvc, or perfect forwarding support and not mingw

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME((_lrc0, kw) in(lrc0))
    BOOST_PARAMETER_NAME((_lr0, kw) in_out(lr0))
    BOOST_PARAMETER_NAME((_rrc0, kw) in(rrc0))
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    BOOST_PARAMETER_NAME((_rr0, kw) consume(rr0))
#else
    BOOST_PARAMETER_NAME((_rr0, kw) rr0)
#endif
    BOOST_PARAMETER_NAME((_lrc1, kw) in(lrc1))
    BOOST_PARAMETER_NAME((_lr1, kw) out(lr1))
    BOOST_PARAMETER_NAME((_rrc1, kw) in(rrc1))
    BOOST_PARAMETER_NAME((_rr1, kw) rr1)
} // namespace test

#include <boost/mpl/bool.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include "evaluate_category.hpp"

#if !defined(__MINGW32__) && defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
#include <boost/parameter/preprocessor.hpp>
#include <utility>
#elif defined(BOOST_MSVC)
#include <boost/parameter/preprocessor.hpp>
#else
#include <boost/parameter/preprocessor_no_spec.hpp>
#endif

namespace test {

    struct C
    {
#if ( \
        !defined(__MINGW32__) && \
        defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) \
    ) || defined(BOOST_MSVC)
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<1> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs0_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<2> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs1_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<3> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs2_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<4> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs3_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<5> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs4_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<6> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs5_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<7> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs6_pred;
        typedef boost::mpl::if_<
            boost::is_convertible<boost::mpl::_,std::bitset<8> >
          , boost::mpl::true_
          , boost::mpl::false_
        > bs7_pred;

        BOOST_PARAMETER_CONST_FUNCTION_CALL_OPERATOR((bool), kw,
            (deduced
                (required
                    (lrc0, *(bs0_pred))
                    (lr0, *(bs1_pred))
                    (rrc0, *(bs2_pred))
                    (rr0, *(bs3_pred))
                    (lrc1, *(bs4_pred))
                )
                (optional
                    (lr1, *(bs5_pred), test::lvalue_bitset<5>())
                    (rrc1, *(bs6_pred), test::rvalue_const_bitset<6>())
                    (rr1, *(bs7_pred), test::rvalue_bitset<7>())
                )
            )
        )
#else
        BOOST_PARAMETER_NO_SPEC_CONST_FUNCTION_CALL_OPERATOR((bool))
#endif  // msvc, or perfect forwarding support and not mingw
        {
#if ( \
        !defined(__MINGW32__) && \
        defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) \
    ) || defined(BOOST_MSVC)
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<0>(lrc0)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , test::U::evaluate_category<1>(lr0)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<4>(lrc1)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , test::U::evaluate_category<5>(lr1)
            );
#else   // mingw, or no perfect forwarding support and not msvc
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<0>(args[test::_lrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , test::U::evaluate_category<1>(args[test::_lr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<4>(args[test::_lrc1])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , test::U::evaluate_category<5>(
                    args[test::_lr1 | test::lvalue_bitset<5>()]
                )
            );
#endif  // msvc, or perfect forwarding support and not mingw
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
#if defined(__MINGW32__)
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , test::U::evaluate_category<2>(args[test::_rrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , test::U::evaluate_category<3>(args[test::_rr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , test::U::evaluate_category<6>(
                    args[test::_rrc1 | test::rvalue_const_bitset<6>()]
                )
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , test::U::evaluate_category<7>(
                    args[test::_rr1 | test::rvalue_bitset<7>()]
                )
            );
#else   // !defined(__MINGW32__)
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , test::U::evaluate_category<2>(std::forward<rrc0_type>(rrc0))
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , test::U::evaluate_category<3>(std::forward<rr0_type>(rr0))
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference_to_const
              , test::U::evaluate_category<6>(std::forward<rrc1_type>(rrc1))
            );
            BOOST_TEST_EQ(
                test::passed_by_rvalue_reference
              , test::U::evaluate_category<7>(std::forward<rr1_type>(rr1))
            );
#endif  // __MINGW32__
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
#if defined(BOOST_MSVC)
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<2>(rrc0)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<3>(rr0)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<6>(rrc1)
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<7>(rr1)
            );
#else   // !defined(BOOST_MSVC)
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<2>(args[test::_rrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<3>(args[test::_rr0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<6>(
                    args[test::_rrc1 | test::rvalue_const_bitset<6>()]
                )
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::U::evaluate_category<7>(
                    args[test::_rr1 | test::rvalue_bitset<7>()]
                )
            );
#endif  // BOOST_MSVC
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING

            return true;
        }
    };
} // namespace test

int main()
{
    test::C cp0;
    test::C cp1;

#if ( \
        !defined(__MINGW32__) && \
        defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) \
    ) || defined(BOOST_MSVC)
    cp0(
        test::lvalue_const_bitset<4>()
      , test::lvalue_const_bitset<0>()
      , test::lvalue_bitset<1>()
      , test::rvalue_const_bitset<2>()
      , test::rvalue_bitset<3>()
    );
    cp0(
        test::lvalue_const_bitset<4>()
      , test::lvalue_const_bitset<0>()
      , test::rvalue_const_bitset<6>()
      , test::lvalue_bitset<1>()
      , test::rvalue_const_bitset<2>()
      , test::rvalue_bitset<3>()
    );
    cp1(
        test::lvalue_bitset<1>()
      , test::rvalue_const_bitset<2>()
      , test::rvalue_bitset<3>()
      , test::lvalue_const_bitset<4>()
      , test::lvalue_bitset<5>()
      , test::rvalue_const_bitset<6>()
      , test::rvalue_bitset<7>()
      , test::lvalue_const_bitset<0>()
    );
#else   // mingw, or no perfect forwarding support and not msvc
    cp0(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );
    cp0(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );
    cp1(
        test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
      , test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lr1 = test::lvalue_bitset<5>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_rr1 = test::rvalue_bitset<7>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
    );
#endif  // msvc, or perfect forwarding support and not mingw
    return boost::report_errors();
}

