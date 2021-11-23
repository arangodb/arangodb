
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/function.hpp>
#include <boost/core/lightweight_test.hpp>

//

boost::function<int(int, int)> get_fn_1();
boost::function2<int, int, int> get_fn_2();

//

int main()
{
    BOOST_TEST_EQ( get_fn_1()( 1, 2 ), 3 );
    BOOST_TEST_EQ( get_fn_2()( 1, 2 ), 3 );

    return boost::report_errors();
}
