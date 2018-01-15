/*=============================================================================
    Copyright (C) 2016 Lee Clagett

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/type_traits/add_const.hpp>
#include <boost/type_traits/add_reference.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_constructible.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>

struct convertible
{
    convertible(int) {}
};

template <typename From, typename To>
bool is_convertible(bool has_conversion)
{
    typedef typename boost::remove_reference<
        typename boost::remove_const<From>::type
    >::type from_rvalue;
    typedef typename boost::add_reference<from_rvalue>::type from_lvalue;
    typedef typename boost::add_const<from_lvalue>::type from_const_lvalue;

    return 
        boost::is_convertible<from_rvalue, To>::value == has_conversion &&
        boost::is_convertible<from_lvalue, To>::value == has_conversion &&
        boost::is_convertible<from_const_lvalue, To>::value == has_conversion;
}

// is_constructible has a few requirements
#if !defined(BOOST_NO_CXX11_DECLTYPE) &&   \
    !defined(BOOST_NO_CXX11_TEMPLATES) &&  \
    !defined(BOOST_NO_SFINAE_EXPR)

#define FUSION_TEST_HAS_CONSTRUCTIBLE

template <typename To, typename... Args>
bool is_lvalue_constructible(bool has_constructor)
{
    return has_constructor ==
        boost::is_constructible<
            To
          , typename boost::add_reference<Args>::type...
        >::value;
}

template <typename To, typename... Args>
bool is_constructible_impl(bool has_constructor)
{
    return
        boost::is_constructible<To, Args...>::value == has_constructor &&
        is_lvalue_constructible<To, Args...>(has_constructor) &&
        is_lvalue_constructible<
            To, typename boost::add_const<Args>::type...
        >(has_constructor);
}

template <typename To, typename... Args>
bool is_constructible(bool has_constructor)
{
    return
        is_constructible_impl<
            To
          , typename boost::remove_reference<
                typename boost::remove_const<Args>::type
            >::type...
        >(has_constructor);
}

void test_constructible()
{
    BOOST_TEST((is_constructible< FUSION_SEQUENCE<> >(true)));

    BOOST_TEST((is_constructible< FUSION_SEQUENCE<int> >(true)));
    BOOST_TEST((is_constructible<FUSION_SEQUENCE<int>, int>(true)));

    BOOST_TEST((is_constructible<FUSION_SEQUENCE<convertible>, int>(true)));
    BOOST_TEST((
        is_constructible<FUSION_SEQUENCE<convertible>, convertible>(true)
    ));
        
    BOOST_TEST((
        is_constructible<FUSION_SEQUENCE<int, int>, int, int>(true)
    ));

    BOOST_TEST((
        is_constructible<FUSION_SEQUENCE<convertible, int>, int, int>(true)
    ));
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<convertible, int>, convertible, int
        >(true)
    ));
    
    BOOST_TEST((
        is_constructible<FUSION_SEQUENCE<int, convertible>, int, int>(true)
    ));
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<int, convertible>, int, convertible
        >(true)
    ));
    
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<convertible, convertible>, int, int
        >(true)
    ));
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<convertible, convertible>, convertible, int
        >(true)
    ));
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<convertible, convertible>, int, convertible
        >(true)
    ));
    BOOST_TEST((
        is_constructible<
            FUSION_SEQUENCE<convertible, convertible>, convertible, convertible
        >(true)
    ));
}

#endif // is_constructible is available

void test_convertible(bool has_seq_conversion)
{
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<> >(false)));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<int> >(false)));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<const int&> >(false)));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<convertible> >(false)));
    BOOST_TEST((
        is_convertible<int, FUSION_SEQUENCE<const convertible&> >(false)
    ));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<int, int> >(false)));
    BOOST_TEST((
        is_convertible<int, FUSION_SEQUENCE<const int&, const int&> >(false)
    ));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<convertible, int> >(false)));
    BOOST_TEST((
        is_convertible<int, FUSION_SEQUENCE<const convertible&, const int&> >(false)
    ));
    BOOST_TEST((is_convertible<int, FUSION_SEQUENCE<int, convertible> >(false)));
    BOOST_TEST((
        is_convertible<int, FUSION_SEQUENCE<const int&, const convertible&> >(false)
    ));
    BOOST_TEST((
        is_convertible<int, FUSION_SEQUENCE<convertible, convertible> >(false)
    ));
    BOOST_TEST((
        is_convertible<
            int, FUSION_SEQUENCE<const convertible&, const convertible&>
        >(false)
    ));

    BOOST_TEST((is_convertible<FUSION_SEQUENCE<>, FUSION_SEQUENCE<> >(true)));
    BOOST_TEST((
        is_convertible<FUSION_SEQUENCE<int>, FUSION_SEQUENCE<int> >(true)
    ));
    BOOST_TEST((
        is_convertible<FUSION_SEQUENCE<int>, FUSION_SEQUENCE<const int&> >(true)
    ));
    BOOST_TEST((
        is_convertible<FUSION_SEQUENCE<int>, FUSION_SEQUENCE<convertible> >(true)
    ));
    BOOST_TEST((
        is_convertible<FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int, int> >(true)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<const int&, const int&>
        >(true)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<convertible, int>
        >(true)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int, convertible>
        >(true)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_SEQUENCE<int, int>
          , FUSION_SEQUENCE<convertible, convertible>
        >(true)
    ));

    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<>, FUSION_SEQUENCE<>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int>, FUSION_SEQUENCE<int>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int>, FUSION_SEQUENCE<const int&>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int>, FUSION_SEQUENCE<convertible>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int, int>, FUSION_SEQUENCE<int, int>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int, int>
          , FUSION_SEQUENCE<const int&, const int&>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int, int>, FUSION_SEQUENCE<convertible, int>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int, int>, FUSION_SEQUENCE<int, convertible>
        >(has_seq_conversion)
    ));
    BOOST_TEST((
        is_convertible<
            FUSION_ALT_SEQUENCE<int, int>
          , FUSION_SEQUENCE<convertible, convertible>
        >(has_seq_conversion)
    ));
}

