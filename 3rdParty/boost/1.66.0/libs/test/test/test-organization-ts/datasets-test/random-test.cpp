//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! @brief tests random dataset
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/generators/random.hpp>
#include <boost/test/data/monomorphic/zip.hpp>
#include <boost/test/data/monomorphic/array.hpp>
#include <boost/test/data/monomorphic/grid.hpp>
#include <boost/test/data/for_each_sample.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_default )
{
    BOOST_TEST( data::random().size() == data::BOOST_TEST_DS_INFINITE_SIZE );

    auto ds = data::random();

    BOOST_CHECK_THROW( data::for_each_sample( ds, [](double ) {}), std::logic_error );

    invocation_count ic;

    ic.m_value = 0;
    data::for_each_sample( ds, ic, 10 );
    BOOST_TEST( ic.m_value == 10 );

    ic.m_value = 0;
    int arr[] = {1,2,3,4,5};
    data::for_each_sample( ds^arr, ic );
    BOOST_TEST( ic.m_value == 5 );

    BOOST_CHECK_THROW( ds * arr, std::logic_error );
    BOOST_CHECK_THROW( arr * ds, std::logic_error );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_uniform_range )
{
    auto ds1 = data::random(1,5);

    data::for_each_sample( ds1, [](int s) {
        BOOST_TEST(s>=1);
        BOOST_TEST(s<=5);
    }, 10);

    auto ds2 = data::random(1.,2.);

    data::for_each_sample( ds2, [](double s) {
        BOOST_TEST(s>=1.);
        BOOST_TEST(s<=2.);
    }, 100);
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_parameterized_init )
{
    auto ds1 = data::random(data::distribution = std::normal_distribution<>(5.,2));
    typedef decltype(ds1) DS1;

    BOOST_TEST(( std::is_same<DS1::generator_type::distr_type,
                                std::normal_distribution<>>::value ));
    BOOST_TEST(( std::is_same<DS1::generator_type::sample,double>::value ));
    BOOST_TEST(( std::is_same<DS1::generator_type::engine_type,
                                std::default_random_engine>::value ));

    auto ds2 = data::random(data::distribution = std::discrete_distribution<>());
    typedef decltype(ds2) DS2;

    BOOST_TEST(( std::is_same<DS2::generator_type::distr_type,
                                std::discrete_distribution<>>::value ));
    BOOST_TEST(( std::is_same<DS2::generator_type::sample,int>::value ));
    BOOST_TEST(( std::is_same<DS2::generator_type::engine_type,
                                std::default_random_engine>::value ));

    auto ds3 = data::random(data::engine = std::minstd_rand());
    typedef decltype(ds3) DS3;

    BOOST_TEST(( std::is_same<DS3::generator_type::distr_type,
                                std::uniform_real_distribution<>>::value ));
    BOOST_TEST(( std::is_same<DS3::generator_type::sample,double>::value ));
    BOOST_TEST(( std::is_same<DS3::generator_type::engine_type,
                                std::minstd_rand>::value ));

    auto ds4 = data::random(data::seed = 100UL);
    typedef decltype(ds4) DS4;

    BOOST_TEST(( std::is_same<DS4::generator_type::distr_type,
                                std::uniform_real_distribution<>>::value ));
    BOOST_TEST(( std::is_same<DS4::generator_type::sample,double>::value ));
    BOOST_TEST(( std::is_same<DS4::generator_type::engine_type,
                                std::default_random_engine>::value ));

    auto ds5 = data::random(data::seed = 100UL);

    std::list<double> vals;
    data::for_each_sample( ds4, [&vals](double s) {
        vals.push_back( s );
    }, 10);
    data::for_each_sample( ds5, [&vals](double s) {
        BOOST_TEST( vals.front() == s );
        vals.pop_front();
    }, 10);

    auto ds6 = data::random(( data::engine = std::minstd_rand(),
                              data::distribution = std::normal_distribution<>(5.,2),
                              data::seed = 20UL ));
    typedef decltype(ds6) DS6;

    BOOST_TEST(( std::is_same<DS6::generator_type::distr_type,
                                std::normal_distribution<>>::value ));
    BOOST_TEST(( std::is_same<DS6::generator_type::sample,double>::value ));
    BOOST_TEST(( std::is_same<DS6::generator_type::engine_type,
                                std::minstd_rand>::value ));
}

//____________________________________________________________________________//

// EOF

