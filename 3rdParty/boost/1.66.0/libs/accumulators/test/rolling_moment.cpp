//  Copyright (C) Eric Niebler 2005.
//  Copyright (C) Pieter Bastiaan Ober 2014.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_moment.hpp>

using namespace boost;
using namespace unit_test;
using namespace accumulators;

template<typename T>
void assert_is_double(T const &)
{
    BOOST_MPL_ASSERT((is_same<T, double>));
}

///////////////////////////////////////////////////////////////////////////////
// test_rolling_moment
//

void test_rolling_second_moment()
{
    accumulator_set<int, stats<tag::rolling_moment<2> > > acc(tag::rolling_moment<2>::window_size = 3);

    acc(2);

    BOOST_CHECK_CLOSE(rolling_moment<2>(acc), 4.0/1 ,1e-5);

    acc(4);

    BOOST_CHECK_CLOSE(rolling_moment<2>(acc), (4.0 + 16.0)/2, 1e-5);

    acc(5);

    BOOST_CHECK_CLOSE(rolling_moment<2>(acc), (4.0 + 16.0 + 25.0)/3, 1e-5);

    acc(6);

    BOOST_CHECK_CLOSE(rolling_moment<2>(acc), (16.0 + 25.0 + 36.0)/3, 1e-5);

    assert_is_double(rolling_moment<2>(acc));
}

void test_rolling_fifth_moment()
{
    accumulator_set<int, stats<tag::rolling_moment<5> > > acc(tag::rolling_moment<2>::window_size = 3);

    acc(2);

    BOOST_CHECK_CLOSE(rolling_moment<5>(acc), 32.0/1, 1e-5);

    acc(3);

    BOOST_CHECK_CLOSE(rolling_moment<5>(acc), (32.0 + 243.0)/2, 1e-5);

    acc(4);

    BOOST_CHECK_CLOSE(rolling_moment<5>(acc), (32.0 + 243.0 + 1024.0)/3, 1e-5);

    acc(5);

    BOOST_CHECK_CLOSE(rolling_moment<5>(acc), (243.0 + 1024.0 + 3125.0)/3, 1e-5);

    assert_is_double(rolling_moment<5>(acc));
}

///////////////////////////////////////////////////////////////////////////////
// init_unit_test_suite
//
test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite *test = BOOST_TEST_SUITE("rolling moment test");

    test->add(BOOST_TEST_CASE(&test_rolling_second_moment));
    test->add(BOOST_TEST_CASE(&test_rolling_fifth_moment));

    return test;
}