//
// Test BOOST_TEST_EQ( p, nullptr )
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#if defined(_MSC_VER)
# pragma warning( disable: 4100 ) // nullptr_t parameter unrereferenced
#endif

#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

int main()
{
#if !defined( BOOST_NO_CXX11_NULLPTR )

    int x = 0;
    int* p1 = 0;
    int* p2 = &x;

    BOOST_TEST_EQ( p1, nullptr );
    BOOST_TEST_NE( p2, nullptr );

    BOOST_TEST_EQ( nullptr, p1 );
    BOOST_TEST_NE( nullptr, p2 );

#endif

    return boost::report_errors();
}
