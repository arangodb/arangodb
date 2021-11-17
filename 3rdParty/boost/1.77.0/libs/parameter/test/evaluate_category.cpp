// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if (BOOST_PARAMETER_MAX_ARITY < 4)
#error Define BOOST_PARAMETER_MAX_ARITY as 4 or greater.
#endif
#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) && \
    (BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY < 5)
#error Define BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY \
as 5 or greater.
#endif

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME((_lrc0, keywords) in(lrc0))
    BOOST_PARAMETER_NAME((_lr0, keywords) in_out(lr0))
    BOOST_PARAMETER_NAME((_rrc0, keywords) in(rrc0))
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    BOOST_PARAMETER_NAME((_rr0, keywords) consume(rr0))
#else
    BOOST_PARAMETER_NAME((_rr0, keywords) rr0)
#endif
} // namespace test

#include <boost/parameter/parameters.hpp>
#include <boost/parameter/required.hpp>

namespace test {

    struct f_parameters
      : boost::parameter::parameters<
            boost::parameter::required<test::keywords::lrc0>
          , boost::parameter::required<test::keywords::lr0>
          , boost::parameter::required<test::keywords::rrc0>
          , boost::parameter::required<test::keywords::rr0>
        >
    {
    };
} // namespace test

#include <boost/core/lightweight_test.hpp>
#include "evaluate_category.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/type_traits/is_scalar.hpp>
#endif

namespace test {

    template <typename T>
    struct B
    {
        template <typename Args>
        static void evaluate(Args const& args)
        {
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::A<T>::evaluate_category(args[test::_lrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference
              , test::A<T>::evaluate_category(args[test::_lr0])
            );
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            if (std::is_scalar<T>::value)
#else
            if (boost::is_scalar<T>::value)
#endif
            {
                BOOST_TEST_EQ(
                    test::passed_by_lvalue_reference_to_const
                  , test::A<T>::evaluate_category(args[test::_rrc0])
                );
                BOOST_TEST_EQ(
                    test::passed_by_lvalue_reference_to_const
                  , test::A<T>::evaluate_category(args[test::_rr0])
                );
            }
            else
            {
                BOOST_TEST_EQ(
                    test::passed_by_rvalue_reference_to_const
                  , test::A<T>::evaluate_category(args[test::_rrc0])
                );
                BOOST_TEST_EQ(
                    test::passed_by_rvalue_reference
                  , test::A<T>::evaluate_category(args[test::_rr0])
                );
            }
#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::A<T>::evaluate_category(args[test::_rrc0])
            );
            BOOST_TEST_EQ(
                test::passed_by_lvalue_reference_to_const
              , test::A<T>::evaluate_category(args[test::_rr0])
            );
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING
        }
    };
} // namespace test

#include <boost/parameter/deduced.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/bind.hpp>
#endif

namespace test {

    struct e_parameters
      : boost::parameter::parameters<
            boost::parameter::required<
                boost::parameter::deduced<test::keywords::lrc0>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mp11::mp_bind<
                    std::is_convertible
                  , boost::mp11::_1
                  , float
                >
#else
              , boost::mpl::if_<
                    boost::is_convertible<boost::mpl::_1,float>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
#endif
            >
          , boost::parameter::required<
                boost::parameter::deduced<test::keywords::lr0>
              , boost::mpl::if_<
                    boost::is_convertible<boost::mpl::_1,char const*>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
            >
          , boost::parameter::required<
                boost::parameter::deduced<test::keywords::rr0>
              , test::string_predicate<test::keywords::lr0>
            >
        >
    {
    };
} // namespace test

#include <boost/parameter/value_type.hpp>

#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/type_traits/remove_const.hpp>
#endif

namespace test {

    struct E
    {
        template <typename Args>
        static void evaluate(Args const& args)
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename std::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::lrc0
                        >::type
                    >::type
                >::evaluate_category(args[test::_lrc0])
            ));
            BOOST_TEST((
                test::passed_by_lvalue_reference == test::A<
                    typename std::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::lr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_lr0])
            ));
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::lrc0
                        >::type
                    >::type
                >::evaluate_category(args[test::_lrc0])
            ));
            BOOST_TEST((
                test::passed_by_lvalue_reference == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::lr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_lr0])
            ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            BOOST_TEST((
                test::passed_by_rvalue_reference == test::A<
                    typename std::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::rr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rr0])
            ));
#elif defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
            BOOST_TEST((
                test::passed_by_rvalue_reference == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::rr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rr0])
            ));
#else   // no MP11 or perfect forwarding support
            BOOST_TEST((
                test::passed_by_lvalue_reference_to_const == test::A<
                    typename boost::remove_const<
                        typename boost::parameter::value_type<
                            Args
                          , test::keywords::rr0
                        >::type
                    >::type
                >::evaluate_category(args[test::_rr0])
            ));
#endif  // MP11 or perfect forwarding support
        }
    };
} // namespace test

int main()
{
    test::B<float>::evaluate(
        test::f_parameters()(
            test::lvalue_const_float()
          , test::lvalue_float()
          , test::rvalue_const_float()
          , test::rvalue_float()
        )
    );
    test::B<char const*>::evaluate(
        test::f_parameters()(
            test::lvalue_const_char_ptr()
          , test::lvalue_char_ptr()
          , test::rvalue_const_char_ptr()
          , test::rvalue_char_ptr()
        )
    );
    test::B<std::string>::evaluate(
        test::f_parameters()(
            test::lvalue_const_str()
          , test::lvalue_str()
          , test::rvalue_const_str()
          , test::rvalue_str()
        )
    );
    test::B<float>::evaluate((
        test::_lr0 = test::lvalue_float()
      , test::_rrc0 = test::rvalue_const_float()
      , test::_rr0 = test::rvalue_float()
      , test::_lrc0 = test::lvalue_const_float()
    ));
    test::B<char const*>::evaluate((
        test::_lr0 = test::lvalue_char_ptr()
      , test::_rrc0 = test::rvalue_const_char_ptr()
      , test::_rr0 = test::rvalue_char_ptr()
      , test::_lrc0 = test::lvalue_const_char_ptr()
    ));
    test::B<std::string>::evaluate((
        test::_lr0 = test::lvalue_str()
      , test::_rrc0 = test::rvalue_const_str()
      , test::_rr0 = test::rvalue_str()
      , test::_lrc0 = test::lvalue_const_str()
    ));

    char baz_arr[4] = "qux";
    typedef char char_arr[4];

#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1800)
    // MSVC-12+ treats static_cast<char_arr&&>(baz_arr) as an lvalue.
#else
    test::B<char_arr>::evaluate(
        test::f_parameters()(
            "crg"
          , baz_arr
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
          , static_cast<char_arr const&&>("uir")
          , static_cast<char_arr&&>(baz_arr)
#else
          , "grl"
          , "grp"
#endif
        )
    );
    test::B<char_arr>::evaluate((
        test::_lr0 = baz_arr
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
      , test::_rrc0 = static_cast<char_arr const&&>("wld")
      , test::_rr0 = static_cast<char_arr&&>(baz_arr)
#else
      , test::_rrc0 = "frd"
      , test::_rr0 = "plg"
#endif
      , test::_lrc0 = "mos"
    ));
#endif  // MSVC-12+

    test::E::evaluate(
        test::e_parameters()(
            test::lvalue_char_ptr()
          , test::rvalue_str()
          , test::lvalue_const_float()
        )
    );
    test::E::evaluate(
        test::e_parameters()(
            test::rvalue_str()
          , test::lvalue_const_float()
          , test::lvalue_char_ptr()
        )
    );
    return boost::report_errors();
}

