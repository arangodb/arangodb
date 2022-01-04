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
//  Description : tests implicit interfaces
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_implicit_for_each )
{
    data::for_each_sample( 2, check_arg_type<int>() );

    data::for_each_sample( "ch", check_arg_type<char const*>() );
    data::for_each_sample( 2., check_arg_type<double>() );
    data::for_each_sample( std::vector<int>( 3 ), check_arg_type<int>() );
    data::for_each_sample( std::list<double>( 2 ), check_arg_type<double>() );
    invocation_count ic;

    ic.m_value = 0;
    data::for_each_sample( std::vector<int>( 3 ), ic );
    BOOST_TEST( ic.m_value == 3 );

    ic.m_value = 0;
    data::for_each_sample( std::list<double>( 2 ), ic, 1 );
    BOOST_TEST( ic.m_value == 1 );

    std::vector<copy_count> samples1( 2 );
    copy_count::value() = 0; // we do not test the construction of the vector
    data::for_each_sample( samples1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    copy_count samples2[] = { copy_count(), copy_count() };
    data::for_each_sample( samples2, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_implicit_join )
{
    auto ds = data::make( 5 );
    BOOST_TEST( (1 + ds).size() == 2 );
    BOOST_TEST( (ds + 1).size() == 2 );

    BOOST_TEST( (1 + data::make( 5 )).size() == 2 );
    BOOST_TEST( (data::make( 5 ) + 1).size() == 2 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_implicit_zip )
{
    auto ds = data::make( 5 );
    BOOST_TEST( (1 ^ ds).size() == 1 );
    BOOST_TEST( (ds ^ 1).size() == 1 );

    BOOST_TEST( (1 ^ data::make( 5 )).size() == 1 );
    BOOST_TEST( (data::make( 5 ) ^ 1).size() == 1 );
}

//____________________________________________________________________________//

// EOF


