//
//  current_function_test2.cpp - a test for boost/current_function.hpp
//
//  Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/current_function.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

int f()
{
    BOOST_TEST_EQ( std::string( BOOST_CURRENT_FUNCTION ).substr( 0, 4 ), std::string( "int " ) );
    return 0;
}

int main()
{
    f();
    return boost::report_errors();
}
