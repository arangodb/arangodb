// Copyright David Abrahams, Daniel Wallin 2005.
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

    BOOST_PARAMETER_NAME(w)
    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)
} // namespace test

#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>

namespace test {

    struct f_predicate
    {
        template <typename T, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<T,int>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
    };
} // namespace test

#endif  // BOOST_PARAMETER_CAN_USE_MP11

#include <boost/parameter/parameters.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/bind.hpp>
#include <type_traits>
#endif

namespace test {

    struct f_parameters // vc6 is happier with inheritance than with a typedef
      : boost::parameter::parameters<
            boost::parameter::required<test::tag::w>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
          , boost::parameter::optional<
                test::tag::x
              , boost::mp11::mp_bind<std::is_convertible,boost::mp11::_1,int>
            >
          , boost::parameter::optional<
                test::tag::y
              , boost::mp11::mp_bind<std::is_convertible,boost::mp11::_1,int>
            >
          , boost::parameter::optional<
                test::tag::z
              , boost::mp11::mp_bind<std::is_convertible,boost::mp11::_1,int>
            >
#else
          , boost::parameter::optional<test::tag::x,test::f_predicate>
          , boost::parameter::optional<test::tag::y,test::f_predicate>
          , boost::parameter::optional<test::tag::z,test::f_predicate>
#endif  // BOOST_PARAMETER_CAN_USE_MP11
        >
    {
    };
} // namespace test

#include <boost/parameter/macros.hpp>
#include <boost/core/lightweight_test.hpp>

namespace test {

#if defined(BOOST_NO_VOID_RETURNS)
    BOOST_PARAMETER_FUN(int, f, 1, 4, f_parameters)
#else
    BOOST_PARAMETER_FUN(void, f, 1, 4, f_parameters)
#endif 
    {
        BOOST_TEST_EQ(p[test::_w][0], p[test::_x | -1]);
        BOOST_TEST_EQ(p[test::_w][1], p[test::_y | -2]);
        BOOST_TEST_EQ(p[test::_w][2], p[test::_z | -3]);
#if defined(BOOST_NO_VOID_RETURNS)
        return 0;
#endif 
    }
} // namespace test

int main()
{
    int a[3];
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    test::f(test::_x = 1, test::_y = 2, test::_z = 3, test::_w = a);
    a[1] = -2;
    a[2] = -3;
    test::f(test::_x = 1, test::_w = a);
    a[0] = -1;
    a[1] = 2;
    test::f(test::_y = 2, test::_w = a);
    a[1] = -2;
    a[2] = 3;
    test::f(test::_z = 3, test::_w = a);
    a[0] = 1;
    test::f(test::_z = 3, test::_x = 1, test::_w = a);
    return boost::report_errors();
}

