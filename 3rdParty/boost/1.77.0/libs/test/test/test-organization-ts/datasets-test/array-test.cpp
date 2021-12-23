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
#include <boost/test/data/monomorphic/array.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_array )
{
    int arr1[] = {1,2,3};
    BOOST_TEST( data::make( arr1 ).size() == 3 );

    double const arr2[] = {7.4,3.2};
    BOOST_TEST( data::make( arr2 ).size() == 2 );

    bool arr3[] = {true, true, false};
    BOOST_TEST( data::make( arr3 ).size() == 3 );

    typedef bool (arr_type)[3];
    arr_type const& arr3_ref = arr3;
    BOOST_TEST( data::make( arr3_ref ).size() == 3 );

    int arr4[] = {7,11,13,17};
    data::for_each_sample( data::make( arr4 ), check_arg_type<int>() );

    int c = 0;
    int* ptr4 = arr4;
    data::for_each_sample( data::make( arr4 ), [&c,ptr4](int i) {
        BOOST_TEST( i == ptr4[c++] );
    });

    data::for_each_sample( data::make( arr4 ), expected_call_count{ 4 } );
    data::for_each_sample( data::make( arr4 ), expected_call_count{ 2 }, 2 );
    data::for_each_sample( data::make( arr4 ), expected_call_count{ 0 }, 0 );

    copy_count::value() = 0;
    copy_count arr5[] = { copy_count(), copy_count() };
    data::for_each_sample( data::make( arr5 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    copy_count const arr6[] = { copy_count(), copy_count() };
    data::for_each_sample( data::make( arr6 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == 0 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_array_make_type )
{
    int arr1[] = {1,2,3};

    typedef int (&arr_t)[3];
    BOOST_STATIC_ASSERT(( boost::is_array< boost::remove_reference<arr_t>::type >::value ) );

    typedef data::result_of::make<int (&)[3]>::type dataset_array_type;
    dataset_array_type res = data::make( arr1 );
    BOOST_TEST( res.size() == 3 );

    double const arr2[] = {7.4,3.2};
    typedef data::result_of::make<double const (&)[2]>::type dataset_array_double_type;
    dataset_array_double_type res2 = data::make( arr2 );

    BOOST_TEST( res2.size() == 2 );
}

// EOF
