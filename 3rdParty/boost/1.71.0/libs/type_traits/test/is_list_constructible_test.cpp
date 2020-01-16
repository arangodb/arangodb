
//  Copyright 2017 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#if defined( __GNUC__ ) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 407)
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_list_constructible.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#if defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_NO_CXX11_DECLTYPE) \
    || defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) || defined(BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS)\
    || BOOST_WORKAROUND(BOOST_GCC, < 40700)

int main() {}

#else

#include <vector>

struct X
{
    int a;
    int b;
};

struct Y
{
    Y( int = 0, int = 0 );
};

struct Z
{
    explicit Z( int = 0, int = 0 );
};

struct V
{
    V( int&, int& );
};

TT_TEST_BEGIN(is_list_constructible)

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int, int, int>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int, char>::value), true);

#if defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 4)
// g++ 4.x doesn't seem to disallow narrowing
#else
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<int, float>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int const, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int, int, int>::value), false);

#if defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 4)
// g++ 4.x doesn't seem to disallow narrowing
#else
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, float>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<X, int, float>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int const, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int, int, int>::value), false);

#if defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 4)
// g++ 4.x doesn't seem to disallow narrowing
#elif defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 7) && (__cplusplus >= 201500)
// g++ 7.1 in -std=c++1z, c++17 has a bug
#else
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, float>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Y, int, float>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int, int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int const, int const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int, int, int>::value), false);

#if defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 4)
// g++ 4.x doesn't seem to disallow narrowing
#elif defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && (__GNUC__ == 7) && (__cplusplus >= 201500)
// g++ 7.1 in -std=c++1z, c++17 has a bug
#else
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, float>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<Z, int, float>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V, int>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V, int, int>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V, int, int, int>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V, int&, int&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_list_constructible<V, int const&, int const&>::value), false);

TT_TEST_END

#endif
