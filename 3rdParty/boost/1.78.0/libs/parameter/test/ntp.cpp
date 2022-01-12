// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if (BOOST_PARAMETER_MAX_ARITY < 4)
#error Define BOOST_PARAMETER_MAX_ARITY as 4 or greater.
#endif

namespace test {

    struct X
    {
    };

    struct Y : X
    {
    };
} // namespace test

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>
#endif

namespace test {

    struct Z
    {
        template <typename T, typename Args>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using fn = std::is_base_of<
            X
          , typename std::remove_const<
                typename std::remove_reference<T>::type
            >::type
        >;
#else
        struct apply
          : boost::mpl::if_<
                boost::is_base_of<
                    X
                  , typename boost::remove_const<
                        typename boost::remove_reference<T>::type
                    >::type
                >
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        {
        };
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    };
} // namespace test

#include <boost/parameter/template_keyword.hpp>

namespace test {

    template <typename T = int>
    struct a0_is : boost::parameter::template_keyword<test::a0_is<>,T>
    {
    };

    template <typename T = int>
    struct a1_is : boost::parameter::template_keyword<test::a1_is<>,T>
    {
    };

    template <typename T = int>
    struct a2_is : boost::parameter::template_keyword<test::a2_is<>,T>
    {
    };

    template <typename T = int>
    struct a3_is : boost::parameter::template_keyword<test::a3_is<>,T>
    {
    };
} // namespace test

#include <boost/parameter/parameters.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/deduced.hpp>
#include <boost/parameter/binding.hpp>

namespace test {

    template <
        typename A0 = boost::parameter::void_
      , typename A1 = boost::parameter::void_
      , typename A2 = boost::parameter::void_
      , typename A3 = boost::parameter::void_
    >
    struct with_ntp
    {
        typedef typename boost::parameter::parameters<
            test::a0_is<>
          , test::a1_is<>
          , test::a2_is<>
          , boost::parameter::optional<
                boost::parameter::deduced<test::a3_is<> >
              , Z
            >
        >::BOOST_NESTED_TEMPLATE bind<
            A0
          , A1
          , A2
          , A3
#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x564))
          , boost::parameter::void_
#endif
        >::type args;
        typedef typename boost::parameter::binding<
            args
          , test::a0_is<>
          , void*
        >::type a0;
        typedef typename boost::parameter::binding<
            args
          , test::a1_is<>
          , void*
        >::type a1;
        typedef typename boost::parameter::binding<
            args
          , test::a2_is<>
          , void*
        >::type a2;
        typedef typename boost::parameter::binding<
            args
          , test::a3_is<>
          , void*
        >::type a3;
        typedef void(*type)(a0, a1, a2, a3);
    };
} // namespace test

#include <boost/mpl/aux_/test.hpp>

#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

MPL_TEST_CASE()
{
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    static_assert(
        std::is_same<
            test::with_ntp<>::type
          , void(*)(void*, void*, void*, void*)
        >::value
      , "type must be void(*)(void*, void*, void*, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<test::a2_is<int> >::type
          , void(*)(void*, void*, int, void*)
        >::value
      , "type must be void(*)(void*, void*, int, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<test::a1_is<int> >::type
          , void(*)(void*, int, void*, void*)
        >::value
      , "type must be void(*)(void*, int, void*, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<test::a2_is<int const>,test::a1_is<float> >::type
          , void(*)(void*, float, int const, void*)
        >::value
      , "type must be void(*)(void*, float, int const, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<int const>::type
          , void(*)(int const, void*, void*, void*)
        >::value
      , "type must be void(*)(int const, void*, void*, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<int,float>::type
          , void(*)(int, float, void*, void*)
        >::value
      , "type must be void(*)(int, float, void*, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<int,float,char>::type
          , void(*)(int, float, char, void*)
        >::value
      , "type must be void(*)(int, float, char, void*)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<test::a0_is<int>,test::Y>::type
          , void(*)(int, void*, void*, test::Y)
        >::value
      , "type must be must be void(*)(int, void*, void*, test::Y)"
    );
    static_assert(
        std::is_same<
            test::with_ntp<int&,test::a2_is<char>,test::Y>::type
          , void(*)(int&, void*, char, test::Y)
        >::value
      , "type must be void(*)(int&, void*, char, test::Y)"
    );
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<>::type
              , void(*)(void*, void*, void*, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<test::a2_is<int> >::type
              , void(*)(void*, void*, int, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<test::a1_is<int> >::type
              , void(*)(void*, int, void*, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<
                    test::a2_is<int const>
                  , test::a1_is<float>
                >::type
              , void(*)(void*, float, int const, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<int const>::type
              , void(*)(int const, void*, void*, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<int,float>::type
              , void(*)(int, float, void*, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<int,float,char>::type
              , void(*)(int, float, char, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<test::a0_is<int>,test::Y>::type
              , void(*)(int, void*, void*, test::Y)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<int&,test::a2_is<char>,test::Y>::type
              , void(*)(int&, void*, char, test::Y)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11

    typedef int test_array[1];
    typedef void(*test_function)();

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    static_assert(
        std::is_same<
            test::with_ntp<test_array,test_function>::type
          , void(*)(test_array&, test_function, void*, void*)
        >::value
      , "type must be void(*)(test_array&, test_function, void*, void*)"
    );
#else
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<
                test::with_ntp<test_array,test_function>::type
              , void(*)(test_array&, test_function, void*, void*)
            >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
#endif
}

