//  Copyright (C) Eric Niebler 2008.
//  Copyright (C) Pieter Bastiaan Ober 2014.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>

using namespace boost;
using namespace unit_test;
using namespace accumulators;

template<typename T>
void assert_is_double(T const &)
{
    BOOST_MPL_ASSERT((is_same<T, double>));
}

// test_rolling_mean_test_impl
// implements a test for window_size = 5
size_t window_size = 5;

template<typename accumulator_set_type>
void
test_rolling_mean_test_impl(accumulator_set_type& acc)
{
    acc(1);
    BOOST_CHECK_CLOSE(1., rolling_mean(acc), 1e-5);

    acc(2);
    BOOST_CHECK_CLOSE(1.5, rolling_mean(acc), 1e-5);

    acc(3);
    BOOST_CHECK_CLOSE(2., rolling_mean(acc), 1e-5);

    acc(4);
    BOOST_CHECK_CLOSE(2.5, rolling_mean(acc), 1e-5);

    acc(5);
    BOOST_CHECK_CLOSE(3., rolling_mean(acc), 1e-5);

    acc(6);
    BOOST_CHECK_CLOSE(4., rolling_mean(acc), 1e-5);

    acc(7);
    BOOST_CHECK_CLOSE(5., rolling_mean(acc), 1e-5);

    assert_is_double(rolling_mean(acc));
}

///////////////////////////////////////////////////////////////////////////////
// test_rolling_mean
void test_rolling_mean()
{
    accumulator_set<int,stats<tag::immediate_rolling_mean> >
        acc_immediate_rolling_mean(tag::immediate_rolling_mean::window_size = window_size),
        acc_immediate_rolling_mean2(tag::immediate_rolling_mean::window_size = window_size, sample = 0);

    accumulator_set<int,stats<tag::rolling_mean(immediate)> >
       acc_immediate_rolling_mean3(tag::immediate_rolling_mean::window_size = window_size);

    accumulator_set<int,stats<tag::lazy_rolling_mean> >
        acc_lazy_rolling_mean(tag::lazy_rolling_mean::window_size = window_size),
        acc_lazy_rolling_mean2(tag::lazy_rolling_mean::window_size = window_size, sample = 0);

    accumulator_set<int,stats<tag::rolling_mean(lazy)> >
       acc_lazy_rolling_mean3(tag::lazy_rolling_mean::window_size = window_size);

    accumulator_set<int,stats<tag::rolling_mean> >
        acc_default_rolling_mean(tag::rolling_mean::window_size = window_size),
        acc_default_rolling_mean2(tag::rolling_mean::window_size = window_size, sample = 0);

    //// test the different implementations
    test_rolling_mean_test_impl(acc_lazy_rolling_mean);
    test_rolling_mean_test_impl(acc_default_rolling_mean);
    test_rolling_mean_test_impl(acc_immediate_rolling_mean);

    test_rolling_mean_test_impl(acc_lazy_rolling_mean2);
    test_rolling_mean_test_impl(acc_default_rolling_mean2);
    test_rolling_mean_test_impl(acc_immediate_rolling_mean2);

    test_rolling_mean_test_impl(acc_lazy_rolling_mean3);
    test_rolling_mean_test_impl(acc_immediate_rolling_mean3);

    //// test that the default implementation is the 'immediate' computation
    BOOST_REQUIRE(sizeof(acc_lazy_rolling_mean) != sizeof(acc_immediate_rolling_mean));
    BOOST_CHECK  (sizeof(acc_default_rolling_mean) == sizeof(acc_immediate_rolling_mean));

    //// test the equivalence of the different ways to indicate a feature
    BOOST_CHECK  (sizeof(acc_lazy_rolling_mean) == sizeof(acc_lazy_rolling_mean2));
    BOOST_CHECK  (sizeof(acc_lazy_rolling_mean) == sizeof(acc_lazy_rolling_mean3));
    BOOST_CHECK  (sizeof(acc_immediate_rolling_mean) == sizeof(acc_immediate_rolling_mean2));
    BOOST_CHECK  (sizeof(acc_immediate_rolling_mean) == sizeof(acc_immediate_rolling_mean3));
}

///////////////////////////////////////////////////////////////////////////////
// init_unit_test_suite
//
test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite *test = BOOST_TEST_SUITE("rolling mean test");

    test->add(BOOST_TEST_CASE(&test_rolling_mean));

    return test;
}
