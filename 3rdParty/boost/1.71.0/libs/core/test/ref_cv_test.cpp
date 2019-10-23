//  ref_cv_test.cpp: ref( x ) where x is of a cv-qualified type
//
//  Copyright (c) 2017 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/ref.hpp>
#include <boost/core/lightweight_test.hpp>

#define BOOST_TEST_REF( x ) BOOST_TEST( &boost::ref( x ).get() == &x )
#define BOOST_TEST_CREF( x ) BOOST_TEST( &boost::cref( x ).get() == &x )

int main()
{
    int x1 = 1;
    int const x2 = 2;
    int volatile x3 = 3;
    int const volatile x4 = 4;

    BOOST_TEST_REF( x1 );
    BOOST_TEST_CREF( x1 );

    BOOST_TEST_REF( x2 );
    BOOST_TEST_CREF( x2 );

    BOOST_TEST_REF( x3 );
    BOOST_TEST_CREF( x3 );

    BOOST_TEST_REF( x4 );
    BOOST_TEST_CREF( x4 );

    return boost::report_errors();
}
