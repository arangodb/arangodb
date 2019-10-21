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
#include <boost/test/data/test_case.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_seed_with_singleton )
{
    BOOST_TEST( (data::ds_detail::seed{} ->* 1).size() == 1 );

    struct TT {};
    BOOST_TEST( (data::ds_detail::seed{} ->* TT{}).size() == 1 );

    int arr[] = {1,2,3};
    BOOST_TEST( (data::ds_detail::seed{} ->* arr).size() == 3 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_seed_with_zip )
{
    int arr[] = {1,2,3};
    BOOST_TEST( (data::ds_detail::seed{} ->* 1 ^ arr).size() == 3 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_seed_with_join )
{
    int arr[] = {1,2,3};
    BOOST_TEST( (data::ds_detail::seed{} ->* 1 + arr).size() == 4 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_seed_with_grid )
{
    int arr1[] = {1,2,3};
    int arr2[] = {1,2,3};
    BOOST_TEST( (data::ds_detail::seed{} ->* arr1 * arr2).size() == 9 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_seed_with_init_list )
{
    BOOST_TEST( (data::ds_detail::seed{} ->* data::make({1,2,3})).size() == 3 );
}

// EOF
