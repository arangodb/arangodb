/*
 [auto_generated]
 libs/numeric/odeint/test/unwrap_boost_reference.cpp

 [begin_description]
 tba.
 [end_description]

 Copyright 2009-2012 Karsten Ahnert
 Copyright 2009-2012 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#define BOOST_TEST_MODULE odeint_unwrap_boost_reference

#include <boost/numeric/odeint/util/unwrap_reference.hpp>
#include <boost/test/unit_test.hpp>

using namespace boost::unit_test;

template< typename T >
void func( T t )
{
    typedef typename boost::numeric::odeint::unwrap_reference< T >::type type;
}

BOOST_AUTO_TEST_SUITE( unwrap_boost_reference_test )

BOOST_AUTO_TEST_CASE( test_case )
{
    int a;
    func( boost::ref( a ) );
    func( a );
}


BOOST_AUTO_TEST_SUITE_END()
