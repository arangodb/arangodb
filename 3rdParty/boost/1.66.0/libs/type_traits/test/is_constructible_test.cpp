
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_constructible.hpp>
#endif


struct non_copy_constructible
{
   non_copy_constructible();
   non_copy_constructible(int);
   non_copy_constructible(double*, double*);
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
   non_copy_constructible(const non_copy_constructible&) = delete;
#endif
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   non_copy_constructible(non_copy_constructible&&);
#endif
};


struct A { };
struct B : A { };

TT_TEST_BEGIN(is_constructible)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B &&, A>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const &&, A>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const &&, A const>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B volatile &&, A>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B volatile &&, A volatile>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const volatile &&, A>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const volatile &&, A const>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const volatile &&, A volatile>::value) , false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const volatile &&, A const volatile>::value) , false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B&, A>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const &, A>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const &, A&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<B const &, A const>::value), false);

BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, int>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, int const>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, int const&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, non_copy_constructible>::value), true, false);
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_DECLTYPE) && !BOOST_WORKAROUND(BOOST_MSVC, < 1800)
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, double*, double*>::value), true, false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, double const*, double*>::value), false);
#endif
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<non_copy_constructible, const non_copy_constructible&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int, const int>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int, const int&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int*, const int&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int*, const int*>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int*, int*>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int*, int[2]>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int*, int[]>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int(*)(int), int&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int(*)(int), int(*)(int)>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int(&)(int), int(int)>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int(int), int(int)>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_constructible<int(int), int(&)(int)>::value), false);

TT_TEST_END

