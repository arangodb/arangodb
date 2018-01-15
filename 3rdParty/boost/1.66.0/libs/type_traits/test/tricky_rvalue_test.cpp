
//  (C) Copyright John Maddock 2010. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#include <boost/detail/workaround.hpp>
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_convertible.hpp>
#  include <boost/type_traits/is_lvalue_reference.hpp>
#  include <boost/type_traits/is_rvalue_reference.hpp>
#  include <boost/type_traits/is_reference.hpp>
#  include <boost/type_traits/is_function.hpp>
#endif

TT_TEST_BEGIN(rvalue_reference_test)

#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && BOOST_WORKAROUND(BOOST_MSVC, <= 1700))

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_reference<int (&&)(int)>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_rvalue_reference<int (&)(int)>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_rvalue_reference<int (&&)(int)>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_convertible<int&&, int&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_convertible<int(), int(&&)()>::value), true);
#endif

#endif

TT_TEST_END








