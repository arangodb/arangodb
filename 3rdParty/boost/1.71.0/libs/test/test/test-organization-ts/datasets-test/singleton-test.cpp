//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// @brief tests singleton dataset
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/singleton.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_singleton )
{
    BOOST_TEST( data::make( 1 ).size() == 1 );
    BOOST_TEST( data::make( 2.3 ).size() == 1 );
    BOOST_TEST( data::make( "1" ).size() == 1 );

    data::for_each_sample( data::make( 2 ), check_arg_type<int>() );
    data::for_each_sample( data::make( "ch" ), check_arg_type<char const*>() );
    data::for_each_sample( data::make( 2. ), check_arg_type<double>() );

    invocation_count ic;

    ic.m_value = 0;
    data::for_each_sample( data::make( 2 ), ic );
    BOOST_TEST( ic.m_value == 1 );

    ic.m_value = 0;
    data::for_each_sample( data::make( 2 ), ic, 2 );
    BOOST_TEST( ic.m_value == 1 );

    ic.m_value = 0;
    data::for_each_sample( data::make( 2 ), ic, 0 );
    BOOST_TEST( ic.m_value == 0 );

    data::for_each_sample( data::make( 2 ), [] (int s) {
        BOOST_TEST( s == 2 );
    });
    int exp_copy_count = 0;

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == exp_copy_count );

    copy_count::value() = 0;
    copy_count cc1;
    data::for_each_sample( data::make( cc1 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == exp_copy_count );

    copy_count::value() = 0;
    copy_count const cc2;
    data::for_each_sample( data::make( cc2 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == exp_copy_count );

    copy_count::value() = 0;
    auto ds1 = data::make( copy_count::make() );
    data::for_each_sample( ds1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == exp_copy_count );

    copy_count::value() = 0;
    auto ds2 = data::make( copy_count::make_const() );
    data::for_each_sample( ds2, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == exp_copy_count );
}

//____________________________________________________________________________//

// EOF
