//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// Tests C array based dataset
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/initializer_list.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_initializer_list )
{
    BOOST_TEST( data::make( {1,2,3} ).size() == 3 );

    BOOST_TEST( data::make( {7.4,3.2} ).size() == 2 );

    BOOST_TEST( data::make( {true, true, false} ).size() == 3 );

    data::for_each_sample( data::make( {7,11,13,17} ), check_arg_type<int>() );

    int c = 0;
    int exp[] = {7,11,13,17};
    data::for_each_sample( data::make( {7,11,13,17} ), [&c,&exp](int i) {
        BOOST_TEST( i == exp[c++] );
    });

    data::for_each_sample( data::make( {1,2,3,4} ), expected_call_count{ 4 } );
    data::for_each_sample( data::make( {1,2,3,4} ), expected_call_count{ 2 }, 2 );
    data::for_each_sample( data::make( {1,2,3,4} ), expected_call_count{ 0 }, 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( { copy_count(), copy_count() } ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );
}

// EOF
