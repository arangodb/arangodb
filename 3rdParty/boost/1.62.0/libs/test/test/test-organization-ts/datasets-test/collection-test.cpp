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
//  Description : tests stl collection based dataset
// ***************************************************************************

// Boost.Test

#include <boost/test/data/monomorphic/singleton.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/for_each_sample.hpp>

#include <boost/test/unit_test.hpp>
namespace utf=boost::unit_test;
namespace data=utf::data;

#include "datasets-test.hpp"
#include <vector>
#include <list>

//____________________________________________________________________________//

template <typename>
struct forwarded_to_collection : std::false_type {};

template <typename T>
struct forwarded_to_collection< data::monomorphic::collection<T> > : std::true_type {};

BOOST_AUTO_TEST_CASE( test_forwarded_to_collection)
{
  {
    std::vector<int> samples1;
    BOOST_TEST(boost::unit_test::is_forward_iterable<decltype(samples1)>::value, "forward iterable");
    BOOST_TEST((forwarded_to_collection<decltype(data::make( samples1 ))>::value),
               "not properly forwarded to a collection");
  }
  {
    int samples1(0);
    utf::ut_detail::ignore_unused_variable_warning( samples1 );

    BOOST_TEST(!boost::unit_test::is_forward_iterable<decltype(samples1)>::value, "forward iterable");
    BOOST_TEST(!(forwarded_to_collection<decltype(data::make( samples1 ))>::value),
               "not properly forwarded to a collection");
  }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_collection_sizes )
{
    BOOST_TEST( data::make( std::vector<int>() ).size() == 0 );
    BOOST_TEST( data::make( std::vector<int>( 3 ) ).size() == 3 );
    BOOST_TEST( data::make( std::list<double>() ).size() == 0 );
    BOOST_TEST( data::make( std::list<double>( 2 ) ).size() == 2 );

    data::for_each_sample( data::make( std::vector<int>( 3 ) ), check_arg_type<int>() );
    data::for_each_sample( data::make( std::list<double>( 2 ) ), check_arg_type<double>() );

    invocation_count ic;

    ic.m_value = 0;
    data::for_each_sample( data::make( std::vector<int>( 3 ) ), ic );
    BOOST_TEST( ic.m_value == 3 );

    ic.m_value = 0;
    data::for_each_sample( data::make( std::list<double>( 2 ) ), ic, 4 );
    BOOST_TEST( ic.m_value == 2 );

    ic.m_value = 0;
    data::for_each_sample( data::make( std::vector<int>( 3 ) ), ic, 1 );
    BOOST_TEST( ic.m_value == 1 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_collection )
{
    std::vector<int> samples1;
    samples1.push_back(5);
    samples1.push_back(567);
    samples1.push_back(13);

    int c = 0;
    data::for_each_sample( data::make( samples1 ), [&c,samples1](int i) {
        BOOST_TEST( i == samples1[c++] );
    });

    std::list<char const*> samples2;
    samples2.push_back("sd");
    samples2.push_back("bg");
    samples2.push_back( "we3eq3" );

    auto it = samples2.begin();
    data::for_each_sample( data::make( samples2 ), [&it](char const* str ) {
        BOOST_TEST( str == *it++ );
    });
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_collection_copies )
{
    // number of copies due to the dataset make
    int exp_copy_count = 0;

    // number of copies due to the vector constructor
    copy_count::value() = 0;
    int std_vector_constructor_count = 0;
    {
      std::vector<copy_count> v(2); // we cannot do better than std::vector constructor
      std_vector_constructor_count = copy_count::value()/2;
    }

    copy_count::value() = 0;
    int std_list_constructor_count = 0;
    {
      std::list<copy_count> v(2); // we cannot do better than std::vector constructor
      std_list_constructor_count = copy_count::value()/2;
    }

    copy_count::value() = 0;

    data::for_each_sample( data::make( std::vector<copy_count>( 2 ) ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == (exp_copy_count + std_vector_constructor_count) * 2);

    copy_count::value() = 0;
    std::vector<copy_count> samples3( 2 );
    data::for_each_sample( data::make( samples3 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == (exp_copy_count + std_vector_constructor_count) * 2);

    copy_count::value() = 0;
    std::vector<copy_count> const samples4( 2 );
    data::for_each_sample( data::make( samples4 ), check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == (exp_copy_count + std_vector_constructor_count) * 2);

    copy_count::value() = 0;
    auto ds1 = data::make( make_copy_count_collection() );
    BOOST_TEST( ds1.size() == 3 );
    BOOST_TEST( copy_count::value() == (exp_copy_count + std_vector_constructor_count) * 3);
    data::for_each_sample( ds1, check_arg_type<copy_count>() );

    copy_count::value() = 0;
    auto ds2 = data::make( make_copy_count_const_collection() );
    BOOST_TEST( ds2.size() == 3 );
    data::for_each_sample( ds2, check_arg_type<copy_count>() );
    BOOST_TEST( copy_count::value() == (exp_copy_count + std_list_constructor_count + 1) * 3 ); // no rvalue constructor for the dataset
}

//____________________________________________________________________________//

// EOF
