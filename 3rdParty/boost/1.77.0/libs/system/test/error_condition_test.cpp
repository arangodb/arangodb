// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_condition.hpp>
#include <boost/core/lightweight_test.hpp>

namespace sys = boost::system;

int main()
{
    sys::error_condition en;

    BOOST_TEST_EQ( en.value(), 0 );
    BOOST_TEST( !en );

    sys::error_condition en2( en );

    BOOST_TEST( en == en2 );
    BOOST_TEST_NOT( en != en2 );

    en2.assign( 1, en.category() );

    BOOST_TEST_EQ( en2.value(), 1 );
    BOOST_TEST( en2 );

    BOOST_TEST_NOT( en == en2 );
    BOOST_TEST( en != en2 );

    return boost::report_errors();
}
