//
// Negative test for BOOST_TEST_EQ on const char*
//
// Copyright (c) 2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wstring-plus-int"
#endif

#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_EQ("xab"+1 , "yab"+1); // compares addresses, not cstrings

    return boost::report_errors();
}
