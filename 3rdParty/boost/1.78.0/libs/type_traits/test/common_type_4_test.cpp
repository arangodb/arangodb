
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/common_type.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"
#include <iostream>

TT_TEST_BEGIN(common_type_4)
{
    // the unary case should be the same as decay

    BOOST_CHECK_TYPE(tt::common_type<void>::type, void);
    BOOST_CHECK_TYPE(tt::common_type<void const>::type, void);
    BOOST_CHECK_TYPE(tt::common_type<void volatile>::type, void);
    BOOST_CHECK_TYPE(tt::common_type<void const volatile>::type, void);

    BOOST_CHECK_TYPE(tt::common_type<char>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char const>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char volatile>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char const volatile>::type, char);

    BOOST_CHECK_TYPE(tt::common_type<char&>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char const&>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char volatile&>::type, char);
    BOOST_CHECK_TYPE(tt::common_type<char const volatile&>::type, char);

    BOOST_CHECK_TYPE(tt::common_type<char[]>::type, char*);
    BOOST_CHECK_TYPE(tt::common_type<char const[]>::type, char const*);
    BOOST_CHECK_TYPE(tt::common_type<char volatile[]>::type, char volatile*);
    BOOST_CHECK_TYPE(tt::common_type<char const volatile[]>::type, char const volatile*);

    BOOST_CHECK_TYPE(tt::common_type<char[2]>::type, char*);
    BOOST_CHECK_TYPE(tt::common_type<char const[2]>::type, char const*);
    BOOST_CHECK_TYPE(tt::common_type<char volatile[2]>::type, char volatile*);
    BOOST_CHECK_TYPE(tt::common_type<char const volatile[2]>::type, char const volatile*);

    BOOST_CHECK_TYPE(tt::common_type<char (&) [2]>::type, char*);
    BOOST_CHECK_TYPE(tt::common_type<char const (&) [2]>::type, char const*);
    BOOST_CHECK_TYPE(tt::common_type<char volatile (&) [2]>::type, char volatile*);
    BOOST_CHECK_TYPE(tt::common_type<char const volatile (&) [2]>::type, char const volatile*);

    BOOST_CHECK_TYPE(tt::common_type<char()>::type, char(*)());

    BOOST_CHECK_TYPE(tt::common_type<UDT()>::type, UDT(*)());
    BOOST_CHECK_TYPE(tt::common_type<UDT const()>::type, UDT const(*)());
#if __cplusplus <= 201703
    // Deprecated in C++20:
    BOOST_CHECK_TYPE(tt::common_type<UDT volatile()>::type, UDT volatile(*)());
    BOOST_CHECK_TYPE(tt::common_type<UDT const volatile()>::type, UDT const volatile(*)());
#endif
}
TT_TEST_END
