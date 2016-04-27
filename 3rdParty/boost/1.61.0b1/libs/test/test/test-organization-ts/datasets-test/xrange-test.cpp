//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : tests singleton dataset
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/generators/xrange.hpp>
#include <boost/test/data/monomorphic/join.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_single_range )
{
    BOOST_TEST( data::xrange( 5 ).size() == 5 );
    BOOST_TEST( data::xrange( 3. ).size() == 3 );
    BOOST_CHECK_THROW( data::xrange( -5 ), std::logic_error );
    BOOST_CHECK_THROW( data::xrange( 0 ), std::logic_error );

    BOOST_TEST( data::xrange( 1, 5 ).size() == 4 );
    BOOST_TEST( data::xrange( -5, 0 ).size() == 5 );
    BOOST_TEST( data::xrange( 1., 7.5 ).size() == 7 );
    BOOST_CHECK_THROW( data::xrange( 0, 0 ), std::logic_error );
    BOOST_CHECK_THROW( data::xrange( 3, 1 ), std::logic_error );

    BOOST_TEST( data::xrange( 3, 9, 2 ).size() == 3 );
    BOOST_TEST( data::xrange( 5, 0, -1 ).size() == 5 );
    BOOST_TEST( data::xrange( 1, 10, 2 ).size() == 5 );
    BOOST_TEST( data::xrange( 1, 10, 3 ).size() == 3 );
    BOOST_TEST( data::xrange( 1, 10, 8 ).size() == 2 );
    BOOST_TEST( data::xrange( 0., 3., 0.4 ).size() == 8 );
    BOOST_TEST( data::xrange( 1e-6, 2.e-6, 1e-9 ).size() == 1000 );

    BOOST_TEST( data::xrange<int>(( data::begin = 9, data::end = 15 )).size() == 6 );
    BOOST_TEST( data::xrange<double>(( data::step = 0.5, data::end = 3 )).size() == 6 );

    int c = 0;
    data::for_each_sample( data::xrange( 3 ), [&c](int a) {
        BOOST_TEST( a == c++ );
    });

    c = 1;
    data::for_each_sample( data::xrange( 1, 10, 2 ), [&c](int a) {
        BOOST_TEST( a == c );
        c += 2;
    });
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_range_join )
{
    auto ds = data::xrange( 1, 4 ) + data::xrange( 7, 11 );

    BOOST_TEST( ds.size() == 7 );

    invocation_count ic;
    ic.m_value = 0;
    data::for_each_sample( ds, ic );
    BOOST_TEST( ic.m_value == 7 );

    int arr[] = {1,2,3,7,8,9,10};
    int* exp = arr;
    int c = 0;

    data::for_each_sample( ds, [&c,exp](int a) {
        BOOST_TEST( a == exp[c++] );
    });
}

//____________________________________________________________________________//

// EOF


