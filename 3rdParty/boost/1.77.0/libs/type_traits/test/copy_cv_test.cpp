
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/copy_cv.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"
#include <iostream>

TT_TEST_BEGIN(copy_cv)
{
    BOOST_CHECK_TYPE3(tt::copy_cv<int, void>::type, int);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const, void>::type, int const);
    BOOST_CHECK_TYPE3(tt::copy_cv<int volatile, void>::type, int volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const volatile, void>::type, int const volatile);

    BOOST_CHECK_TYPE3(tt::copy_cv<int, void const>::type, int const);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const, void const>::type, int const);
    BOOST_CHECK_TYPE3(tt::copy_cv<int volatile, void const>::type, int const volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const volatile, void const>::type, int const volatile);

    BOOST_CHECK_TYPE3(tt::copy_cv<int, void volatile>::type, int volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const, void volatile>::type, int const volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int volatile, void volatile>::type, int volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const volatile, void volatile>::type, int const volatile);

    BOOST_CHECK_TYPE3(tt::copy_cv<int, void const volatile>::type, int const volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const, void const volatile>::type, int const volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int volatile, void const volatile>::type, int const volatile);
    BOOST_CHECK_TYPE3(tt::copy_cv<int const volatile, void const volatile>::type, int const volatile);

    BOOST_CHECK_TYPE3(tt::copy_cv<int&, void const volatile>::type, int&);

    BOOST_CHECK_TYPE3(tt::copy_cv<int const*, void volatile>::type, int const* volatile);

    BOOST_CHECK_TYPE3(tt::copy_cv<long, int const volatile&>::type, long);
}
TT_TEST_END
