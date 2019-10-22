// Copyright Frank Mori Hess 2009.
// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/template_keyword.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/parameter/required.hpp>
#include <boost/parameter/value_type.hpp>
#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1920)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

namespace test {
    namespace keywords {

        BOOST_PARAMETER_TEMPLATE_KEYWORD(function_type)
    } // namespace keywords

    template <typename K, typename A>
#if BOOST_WORKAROUND(BOOST_MSVC, < 1920)
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    using X = boost::parameter::value_type<
#else
    struct X
      : boost::parameter::value_type<
#endif
            typename boost::parameter::parameters<
                boost::parameter::required<K>
            >::BOOST_NESTED_TEMPLATE bind<A>::type
          , K
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    >;
#else
        >
    {
    };
#endif
#else   // MSVC-14.2
    struct X
    {
        typedef typename boost::parameter::value_type<
            typename boost::parameter::parameters<
                boost::parameter::required<K>
            >::BOOST_NESTED_TEMPLATE bind<A>::type
          , K
        >::type type;
    };
#endif

    template <typename T>
#if BOOST_WORKAROUND(BOOST_MSVC, < 1920)
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    using Y = std::is_same<
#else
    struct Y
      : boost::mpl::if_<
            boost::is_same<
#endif
                T
              , typename X<
                    test::keywords::tag::function_type
                  , test::keywords::function_type<T>
                >::type
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    >;
#else
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    {
    };
#endif
#else   // MSVC-14.2
    struct Y
    {
        typedef typename boost::mpl::if_<
            boost::is_same<
                T
              , typename X<
                    test::keywords::tag::function_type
                  , test::keywords::function_type<T>
                >::type
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type type;
    };
#endif

    struct Z
    {
        int operator()() const
        {
            return 0;
        }
    };
} // namespace test

#include <boost/mpl/aux_/test.hpp>

#if !defined(BOOST_PARAMETER_CAN_USE_MP11) || \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1920)
#include <boost/mpl/assert.hpp>
#endif

MPL_TEST_CASE()
{
#if BOOST_WORKAROUND(BOOST_MSVC, < 1920)
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    static_assert(test::Y<void()>::value, "void()");
    static_assert(test::Y<test::Z>::value, "test::Z");
    static_assert(test::Y<double(double)>::value, "double(double)");
#else
    BOOST_MPL_ASSERT((test::Y<void()>));
    BOOST_MPL_ASSERT((test::Y<test::Z>));
    BOOST_MPL_ASSERT((test::Y<double(double)>));
#endif
#else   // MSVC-14.2
    BOOST_MPL_ASSERT((test::Y<void()>::type));
    BOOST_MPL_ASSERT((test::Y<test::Z>::type));
    BOOST_MPL_ASSERT((test::Y<double(double)>::type));
#endif
}

