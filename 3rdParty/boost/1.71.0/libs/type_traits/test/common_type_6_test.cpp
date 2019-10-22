
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

struct X {};
struct Y: X {};

TT_TEST_BEGIN(common_type_6)
{
    // binary case

    BOOST_CHECK_TYPE3(tt::common_type<void, void>::type, void);

    BOOST_CHECK_TYPE3(tt::common_type<int, int>::type, int);
    BOOST_CHECK_TYPE3(tt::common_type<int&, int&>::type, int);
    BOOST_CHECK_TYPE3(tt::common_type<int&, int const&>::type, int);

    BOOST_CHECK_TYPE3(tt::common_type<X, X>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X&, X&>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X&, X const&>::type, X);

    BOOST_CHECK_TYPE3(tt::common_type<X, Y>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X&, Y&>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X const&, Y&>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X&, Y const&>::type, X);
    BOOST_CHECK_TYPE3(tt::common_type<X const&, Y const&>::type, X);
}
TT_TEST_END
