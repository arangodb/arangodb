// Copyright Cromwell D. Enage 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) && \
    (BOOST_PARAMETER_COMPOSE_MAX_ARITY < 8)
#error Define BOOST_PARAMETER_COMPOSE_MAX_ARITY as 8 or greater.
#endif

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME((_lrc0, kw0) in(lrc0))
    BOOST_PARAMETER_NAME((_lr0, kw1) in_out(lr0))
    BOOST_PARAMETER_NAME((_rrc0, kw2) in(rrc0))
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    BOOST_PARAMETER_NAME((_rr0, kw3) consume(rr0))
#else
    BOOST_PARAMETER_NAME((_rr0, kw3) rr0)
#endif
    BOOST_PARAMETER_NAME((_lrc1, kw4) in(lrc1))
    BOOST_PARAMETER_NAME((_lr1, kw5) out(lr1))
    BOOST_PARAMETER_NAME((_rrc1, kw6) in(rrc1))
    BOOST_PARAMETER_NAME((_rr1, kw7) rr1)
} // namespace test

#include <boost/parameter/preprocessor_no_spec.hpp>
#include <boost/parameter/value_type.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/type_traits/remove_const.hpp>
#include "evaluate_category.hpp"

namespace test {

    BOOST_PARAMETER_NO_SPEC_FUNCTION((bool), evaluate)
    {
        BOOST_TEST((
            test::passed_by_lvalue_reference_to_const == test::A<
                typename boost::remove_const<
                    typename boost::parameter::value_type<
                        Args
                      , test::kw0::lrc0
                    >::type
                >::type
            >::evaluate_category(args[test::_lrc0])
        ));
        BOOST_TEST((
            test::passed_by_lvalue_reference == test::A<
                typename boost::remove_const<
                    typename boost::parameter::value_type<
                        Args
                      , test::kw1::lr0
                    >::type
                >::type
            >::evaluate_category(args[test::_lr0])
        ));

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
        if (
            boost::is_scalar<
                typename boost::remove_const<
                    typename boost::parameter::value_type<
                        Args
                      , test::kw2::rrc0
                    >::type
                >::type
            >::value
        )
        {
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw2::rrc0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rrc0])
            ));
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw3::rr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rr0])
            ));
        }
        else // rrc0's value type isn't scalar
        {
            BOOST_TEST((
                test::passed_by_rvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw2::rrc0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rrc0])
            ));
            BOOST_TEST((
                test::passed_by_rvalue_reference == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw3::rr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rr0])
            ));
        }
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
        BOOST_TEST((
            test::passed_by_lvalue_reference_to_const == test::A<
                typename boost::remove_const<
                    typename boost::parameter::value_type<
                        Args
                      , test::kw2::rrc0
                    >::type
                >::type
            >::evaluate_category(args[test::_rrc0])
        ));
        BOOST_TEST((
            test::passed_by_lvalue_reference_to_const == test::A<
                typename boost::remove_const<
                    typename boost::parameter::value_type<
                        Args
                      , test::kw3::rr0
                    >::type
                >::type
            >::evaluate_category(args[test::_rr0])
        ));
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING

        return true;
    }
} // namespace test

#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>

#if !defined(BOOST_NO_SFINAE)
#include <boost/parameter/aux_/preprocessor/nullptr.hpp>
#include <boost/core/enable_if.hpp>
#include <boost/type_traits/is_base_of.hpp>
#endif

namespace test {

    char const* baz = "baz";

    struct B
    {
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1800)
        B()
        {
        }
#endif

        template <typename Args>
        explicit B(
            Args const& args
#if !defined(BOOST_NO_SFINAE)
          , typename boost::disable_if<
                typename boost::mpl::if_<
                    boost::is_base_of<B,Args>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            >::type* = BOOST_PARAMETER_AUX_PP_NULLPTR
#endif  // BOOST_NO_SFINAE
        )
        {
            test::evaluate(
                test::_lrc0 = args[test::_lrc0]
              , test::_lr0 = args[test::_lr0]
              , test::_rrc0 = args[test::_rrc0]
              , test::_rr0 = args[test::_rr0]
            );
        }

        BOOST_PARAMETER_NO_SPEC_MEMBER_FUNCTION((bool), static evaluate)
        {
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw0::lrc0
                        >::type
                    >::type
                >::evaluate_category(args[test::_lrc0])
            ));
            BOOST_TEST((
                test::passed_by_lvalue_reference == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw1::lr0
                          , char const*
                        >::type
                    >::type
                >::evaluate_category(args[test::_lr0 | test::baz])
            ));
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw2::rrc0
                          , float
                        >::type
                    >::type
                >::evaluate_category(args[test::_rrc0 | 0.0f])
            ));

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST((
                test::passed_by_rvalue_reference == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw3::rr0
                          , std::string
                        >::type
                    >::type
                >::evaluate_category(
                    args[
                        test::_rr0 | std::string(args[test::_lr0 | test::baz])
                    ]
                )
            ));
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::kw3::rr0
                          , std::string
                        >::type
                    >::type
                >::evaluate_category(
                    args[
                        test::_rr0 | std::string(args[test::_lr0 | test::baz])
                    ]
                )
            ));
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING

            return true;
        }
    };

    struct C : B
    {
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1800)
        C() : B()
        {
        }
#endif

        BOOST_PARAMETER_NO_SPEC_CONSTRUCTOR(C, (B))
    };

    struct D
    {
        BOOST_PARAMETER_NO_SPEC_NO_BASE_CONSTRUCTOR(D, D::_evaluate)

        BOOST_PARAMETER_NO_SPEC_CONST_MEMBER_FUNCTION((bool), evaluate_m)
        {
            return D::_evaluate(args);
        }

        BOOST_PARAMETER_NO_SPEC_CONST_FUNCTION_CALL_OPERATOR((bool))
        {
            return D::_evaluate(args);
        }

     private:
        template <typename Args>
        static bool _evaluate(Args const& args)
        {
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
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
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
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
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
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING

            return true;
        }
    };
} // namespace test

int main()
{
    test::evaluate(
        test::_lr0 = test::lvalue_float()
      , test::_rrc0 = test::rvalue_const_float()
      , test::_rr0 = test::rvalue_float()
      , test::_lrc0 = test::lvalue_const_float()
    );
    test::evaluate(
        test::_lr0 = test::lvalue_char_ptr()
      , test::_rrc0 = test::rvalue_const_char_ptr()
      , test::_rr0 = test::rvalue_char_ptr()
      , test::_lrc0 = test::lvalue_const_char_ptr()
    );
    test::evaluate(
        test::_lr0 = test::lvalue_str()
      , test::_rrc0 = test::rvalue_const_str()
      , test::_rr0 = test::rvalue_str()
      , test::_lrc0 = test::lvalue_const_str()
    );

    test::C cf1(
        test::_lr0 = test::lvalue_float()
      , test::_rrc0 = test::rvalue_const_float()
      , test::_rr0 = test::rvalue_float()
      , test::_lrc0 = test::lvalue_const_float()
    );
    test::C cc1(
        test::_lr0 = test::lvalue_char_ptr()
      , test::_rrc0 = test::rvalue_const_char_ptr()
      , test::_rr0 = test::rvalue_char_ptr()
      , test::_lrc0 = test::lvalue_const_char_ptr()
    );
    test::C cs1(
        test::_lr0 = test::lvalue_str()
      , test::_rrc0 = test::rvalue_const_str()
      , test::_rr0 = test::rvalue_str()
      , test::_lrc0 = test::lvalue_const_str()
    );

    char baz_arr[4] = "qux";
    typedef char char_arr[4];

#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && ( \
        BOOST_WORKAROUND(BOOST_GCC, < 40000) || \
        BOOST_WORKAROUND(BOOST_MSVC, >= 1800) \
    )
    // MSVC-12+ treats static_cast<char_arr&&>(baz_arr) as an lvalue.
    // GCC 3- tries to bind string literals
    // to non-const references to char const*.
#else
    test::evaluate(
        test::_lr0 = baz_arr
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
      , test::_rrc0 = static_cast<char_arr const&&>("def")
      , test::_rr0 = static_cast<char_arr&&>(baz_arr)
#else
      , test::_rrc0 = "grl"
      , test::_rr0 = "grp"
#endif
      , test::_lrc0 = "wld"
    );
#endif  // MSVC-12+
    test::B::evaluate(test::_lrc0 = test::lvalue_const_str()[0]);
    test::C::evaluate(
        test::_rrc0 = test::rvalue_const_float()
      , test::_lrc0 = test::lvalue_const_str()[0]
    );

#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1800)
    // MSVC-12+ treats static_cast<char_arr&&>(baz_arr) as an lvalue.
    test::C cp0;
    test::C cp1;
#else
    test::C cp0(
        test::_lrc0 = "frd"
      , test::_lr0 = baz_arr
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
      , test::_rrc0 = static_cast<char_arr const&&>("dfs")
      , test::_rr0 = static_cast<char_arr&&>(baz_arr)
#else
      , test::_rrc0 = "plg"
      , test::_rr0 = "thd"
#endif
    );
    test::C cp1(
        test::_lr0 = baz_arr
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
      , test::_rrc0 = static_cast<char_arr const&&>("dgx")
      , test::_rr0 = static_cast<char_arr&&>(baz_arr)
#else
      , test::_rrc0 = "hnk"
      , test::_rr0 = "xzz"
#endif
      , test::_lrc0 = "zts"
    );
#endif  // MSVC-12+

    cp0.evaluate(
        test::_lrc0 = test::lvalue_const_str()[0]
      , test::_lr0 = test::lvalue_char_ptr()
      , test::_rr0 = test::rvalue_str()
      , test::_rrc0 = test::rvalue_const_float()
    );
    cp1.evaluate(
        test::_lrc0 = test::lvalue_const_str()[0]
      , test::_rr0 = test::rvalue_str()
      , test::_rrc0 = test::rvalue_const_float()
      , test::_lr0 = test::lvalue_char_ptr()
    );

    test::D dp0(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );
    test::D dp1(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );

    dp0.evaluate_m(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );
    dp1.evaluate_m(
        test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
      , test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lr1 = test::lvalue_bitset<5>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_rr1 = test::rvalue_bitset<7>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
    );
    dp0(
        test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
      , test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
    );
    dp1(
        test::_lr0 = test::lvalue_bitset<1>()
      , test::_rrc0 = test::rvalue_const_bitset<2>()
      , test::_rr0 = test::rvalue_bitset<3>()
      , test::_lrc1 = test::lvalue_const_bitset<4>()
      , test::_lr1 = test::lvalue_bitset<5>()
      , test::_rrc1 = test::rvalue_const_bitset<6>()
      , test::_rr1 = test::rvalue_bitset<7>()
      , test::_lrc0 = test::lvalue_const_bitset<0>()
    );
    return boost::report_errors();
}

