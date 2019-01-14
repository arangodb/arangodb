//
// Negative test for BOOST_TEST_CSTR_EQ
//
// Copyright (c) 2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_CSTR_EQ("x" , "y");

    return boost::report_errors();
}
