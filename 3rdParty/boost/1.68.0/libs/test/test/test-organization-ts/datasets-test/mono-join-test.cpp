//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// @brief tests monomorphic join
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/join.hpp>
#include <boost/test/data/monomorphic/singleton.hpp>
#include <boost/test/data/monomorphic/array.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_mono_join )
{
    BOOST_TEST( (data::make( 1 ) + data::make( 5 )).size() == 2 );
    BOOST_TEST( (data::make( std::vector<int>(2) ) + data::make( std::list<int>(3) )).size() == 5 );

    int arr1[] = {1,2};
    int arr2[] = {7,19};
    BOOST_TEST( (data::make( arr1 ) + data::make( arr2 )).size() == 4 );

    BOOST_TEST( (data::make( 1 ) + data::make( 5 ) + data::make( 19 )).size() == 3 );

    BOOST_TEST( (data::make( std::vector<int>(2) ) + data::make( 1 ) + data::make( arr2 ) + data::make( 17 )).size() == 6 );

    int expected[] = {1,2,5,7,19,37};
    int* exp = expected;
    int c = 0;
    invocation_count ic;

    auto samples1 = data::make( std::vector<int>(arr1,arr1+2) ) + data::make( 5 ) + data::make( arr2 ) + data::make( 37 );

    ic.m_value = 0;
    data::for_each_sample( samples1, ic );
    BOOST_TEST( ic.m_value == 6 );

    c = 0;
    data::for_each_sample( samples1, [&c,exp](int i) {
        BOOST_TEST( i == exp[c++] );
    });

    auto samples2 = data::make( std::vector<int>(arr1,arr1+2) ) + (data::make( 5 ) + (data::make( arr2 ) + data::make( 37 )));

    ic.m_value = 0;
    data::for_each_sample( samples2, ic );
    BOOST_TEST( ic.m_value == 6 );

    c = 0;
    data::for_each_sample( samples2, [&c,exp](int i) {
        BOOST_TEST( i == exp[c++] );
    });

    auto samples3 = (data::make( std::vector<int>(arr1,arr1+2) ) + data::make( 5 )) + (data::make( arr2 ) + data::make( 37 ));

    ic.m_value = 0;
    data::for_each_sample( samples3, ic );
    BOOST_TEST( ic.m_value == 6 );

    c = 0;
    data::for_each_sample( samples3, [&c,exp](int i) {
        BOOST_TEST( i == exp[c++] );
    });

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) + data::make( copy_count() ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) + data::make( copy_count() ) + data::make( copy_count() ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) + (data::make( copy_count() ) + data::make( copy_count() )), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );
    copy_count::value() = 0;
    data::for_each_sample( (data::make( copy_count() ) + data::make( copy_count() )) +
                           (data::make( copy_count() ) + data::make( copy_count() )), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    auto ds1        = data::make( copy_count() );
    auto const ds2  = data::make( copy_count() );

    copy_count::value() = 0;
    data::for_each_sample( ds1 + ds1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds2 + ds2, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 + ds2, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    auto jn1 = ds1 + data::make( copy_count() );
    data::for_each_sample( jn1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) + ds1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 + ds2 + ds1, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 + (ds1 + ds2), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( (ds1 + ds1) + (ds2 + ds2), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    int std_vector_constructor_count = 0;
    {
      std::vector<copy_count> v(2); // we cannot do better than std::vector constructor
      std_vector_constructor_count = copy_count::value()/2;
    }

    copy_count::value() = 0;
    auto ds3 = data::make( make_copy_count_collection() ) + data::make( make_copy_count_collection() );
    BOOST_TEST( ds3.size() == 6 );
    data::for_each_sample( ds3, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == std_vector_constructor_count * 2 * 3 );
}

//____________________________________________________________________________//

// EOF

