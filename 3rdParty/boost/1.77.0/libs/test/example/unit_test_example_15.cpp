//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE data driven test example
#include <boost/test/included/unit_test.hpp>

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
namespace data=boost::unit_test::data;
namespace bt=boost::unit_test;

//____________________________________________________________________________//

double x_samples[] = {1.1,2.1,3.1,4.1};
double y_samples[] = {10.1,9.1,8.1};

auto& D = * bt::tolerance(1e-1);
BOOST_DATA_TEST_CASE( data_driven_test, data::make(x_samples) * y_samples, x, y )
{
    BOOST_TEST( x*y < 32.4 );
}

//____________________________________________________________________________//

// EOF
