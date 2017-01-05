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
//  Description : tests monomorphic zip
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/zip.hpp>
#include <boost/test/data/monomorphic/singleton.hpp>
#include <boost/test/data/monomorphic/array.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_mono_zip_sizes )
{
    BOOST_TEST( (data::make( 1 ) ^ data::make( 5 )).size() == 1 );
    BOOST_TEST( (data::make( std::vector<int>(2) ) ^ data::make( std::list<float>(2) )).size() == 2 );
    BOOST_TEST( (data::make( std::vector<int>(2) ) ^ data::make( 5. )).size() == 2 );
    BOOST_TEST( (data::make( std::vector<int>(3) ) ^ data::make( std::list<int>(1) )).size() == 3 );

    BOOST_TEST( (data::make( std::vector<int>(3) ) ^ data::make( std::list<std::string>(3) ) ^ data::make( 5 )).size() == 3 );
    BOOST_TEST( (data::make( std::vector<int>(1) ) ^ data::make( std::list<int>(3) ) ^ data::make( 5 )).size() == 3 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_mono_zip )
{
    int arr1[]         = {1,2};
    char const* arr2[] = {"a","b"};
    int* exp1 = arr1;
    char const** exp2 = arr2;
    int c = 0;
    invocation_count ic;

    auto samples1 = data::make( arr1 ) ^ data::make( arr2 );

    BOOST_TEST( samples1.size() == 2 );

    ic.m_value = 0;
    data::for_each_sample( samples1, ic );
    BOOST_TEST( ic.m_value == 2 );

    data::for_each_sample( samples1, check_arg_type_like<std::tuple<int,char const*>>() );

    c = 0;
    data::for_each_sample( samples1, [&c,exp1,exp2](int i,char const* s) {
        BOOST_TEST( i == exp1[c] );
        BOOST_TEST( s == exp2[c] );
        ++c;
    });

    std::vector<double> vec1;
    vec1.push_back(2.1);
    vec1.push_back(3.2);
    vec1.push_back(4.7);
    int arr3[] = {4,2,1};

    auto samples2 = data::make( vec1 ) ^ data::make( "qqq" ) ^ data::make( arr3 );

    BOOST_TEST( samples2.size() == 3 );

    ic.m_value = 0;
    data::for_each_sample( samples2, ic );
    BOOST_TEST( ic.m_value == 3 );

    data::for_each_sample( samples2, check_arg_type_like<std::tuple<double,char const*,int>>() );

    c = 0;
    int* exp3 = arr3;

    data::for_each_sample( samples2, [&c,&vec1,exp3](double a1,char const* a2,int a3) {
        BOOST_TEST( a1 == vec1[c] );
        BOOST_TEST( a2 == "qqq" );
        BOOST_TEST( a3 == exp3[c] );
        ++c;
    });

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) ^ data::make( copy_count() ), check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) ^ data::make( copy_count() ) ^ data::make( copy_count() ),
                           check_arg_type<std::tuple<copy_count,copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) ^ (data::make( copy_count() ) ^ data::make( copy_count() )),
                           check_arg_type<std::tuple<copy_count,copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    auto ds1        = data::make( copy_count() );
    auto const ds2  = data::make( copy_count() );

    copy_count::value() = 0;
    data::for_each_sample( ds1 ^ ds1, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds2 ^ ds2, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 ^ ds2, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    auto zp1 = ds1 ^ data::make( copy_count() );
    BOOST_TEST( zp1.size() == 1 );
    data::for_each_sample( zp1, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( data::make( copy_count() ) ^ ds1, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 ^ ds2 ^ ds1, check_arg_type<std::tuple<copy_count,copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );

    copy_count::value() = 0;
    data::for_each_sample( ds1 ^ (ds1 ^ ds2), check_arg_type<std::tuple<copy_count,copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == 0 );



    copy_count::value() = 0;
    int std_vector_constructor_count = 0;
    {
      std::vector<copy_count> v(2); // we cannot do better than std::vector constructor
      std_vector_constructor_count = copy_count::value()/2;
    }

    copy_count::value() = 0;
    auto ds3 = data::make( make_copy_count_collection() ) ^ data::make( make_copy_count_collection() );
    BOOST_TEST( ds3.size() == 3 );
    data::for_each_sample( ds3, check_arg_type<std::tuple<copy_count,copy_count>>() );
    BOOST_TEST( copy_count::value() == std_vector_constructor_count *2 *3 );
}

//____________________________________________________________________________//

// EOF
