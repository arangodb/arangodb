//  (C) Copyright Pieter Bastiaan Ober 2014.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <sstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/accumulators/statistics/rolling_variance.hpp>

using namespace boost;
using namespace unit_test;
using namespace accumulators;

template<typename T>
void assert_is_double(T const &)
{
    BOOST_MPL_ASSERT((is_same<T, double>));
}

/*
REFERENCE VALUES PROVIDED BY OCTAVE:

x=[1.2 2.3 3.4 4.5 0.4 2.2 7.1 4.0]

v1_2 = var(x(1:2))
v1_3 = var(x(1:3))
v1_4 = var(x(1:4))
v2_5 = var(x(2:5))
v3_6 = var(x(3:6))
v4_7 = var(x(4:7))
v5_8 = var(x(5:8))

GIVES:

v1_2 =  0.605000000000000
v1_3 =  1.21000000000000
v1_4 =  2.01666666666667
v2_5 =  3.05666666666667
v3_6 =  3.08250000000000
v4_7 =  8.41666666666667
v5_8 =  8.16250000000000
*/

///////////////////////////////////////////////////////////////////////////////
// rolling_variance_test_impl
// implements a test for window_size = 4

size_t window_size = 4;

template<typename accumulator_set_type>
void rolling_variance_test_impl(accumulator_set_type& acc)
{
    // Window contains x(1), value is zero
    acc(1.2);
    BOOST_CHECK_CLOSE(rolling_variance(acc),0.0,1e-10);

    // Window contains x(1)...x(2)
    acc(2.3);
    BOOST_CHECK_CLOSE(rolling_variance(acc),0.605,1e-10);

    // Window contains x(1)...x(3)
    acc(3.4);
    BOOST_CHECK_CLOSE(rolling_variance(acc),1.21,1e-10);

    // Window contains x(1)...x(4)
    acc(4.5);
    BOOST_CHECK_CLOSE(rolling_variance(acc),2.01666666666667,1e-10);

    // Window contains x(2)...x(5)
    acc(0.4);
    BOOST_CHECK_CLOSE(rolling_variance(acc),3.05666666666667,1e-10);

    // Window contains x(3)...x(6)
    acc(2.2);
    BOOST_CHECK_CLOSE(rolling_variance(acc),3.08250000000000,1e-10);

    // Window contains x(4)...x(7)
    acc(7.1);
    BOOST_CHECK_CLOSE(rolling_variance(acc),8.41666666666667,1e-10);

    // Window contains x(5)...x(8)
    acc(4.0);
    BOOST_CHECK_CLOSE(rolling_variance(acc),8.16250000000000,1e-10);

    assert_is_double(rolling_variance(acc));
}

///////////////////////////////////////////////////////////////////////////////
// test_rolling_variance
//
void test_rolling_variance()
{
    // tag::rolling_window::window_size
    accumulator_set<double, stats<tag::immediate_rolling_variance> >
        acc_immediate_rolling_variance(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::immediate_rolling_variance, tag::rolling_mean> >
        acc_immediate_rolling_variance2(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance(immediate)> >
        acc_immediate_rolling_variance3(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::lazy_rolling_variance> >
        acc_lazy_rolling_variance(tag::lazy_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance(lazy)> >
       acc_lazy_rolling_variance2(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance> >
        acc_default_rolling_variance(tag::rolling_variance::window_size = window_size);

    //// test the different implementations
    rolling_variance_test_impl(acc_immediate_rolling_variance);
    rolling_variance_test_impl(acc_immediate_rolling_variance2);
    rolling_variance_test_impl(acc_immediate_rolling_variance3);
    rolling_variance_test_impl(acc_lazy_rolling_variance);
    rolling_variance_test_impl(acc_lazy_rolling_variance2);
    rolling_variance_test_impl(acc_default_rolling_variance);

    //// test that the default implementation is the 'immediate' computation
    BOOST_REQUIRE(sizeof(acc_lazy_rolling_variance) != sizeof(acc_immediate_rolling_variance));
    BOOST_CHECK  (sizeof(acc_default_rolling_variance) == sizeof(acc_immediate_rolling_variance));

    //// test the equivalence of the different ways to indicate a feature
    BOOST_CHECK  (sizeof(acc_immediate_rolling_variance) == sizeof(acc_immediate_rolling_variance2));
    BOOST_CHECK  (sizeof(acc_immediate_rolling_variance) == sizeof(acc_immediate_rolling_variance3));
    BOOST_CHECK  (sizeof(acc_lazy_rolling_variance) == sizeof(acc_lazy_rolling_variance2));
}

///////////////////////////////////////////////////////////////////////////////
// test_persistency_impl
//
template<typename accumulator_set_type>
void test_persistency_impl(accumulator_set_type& acc)
{
    std::stringstream ss;
    {
        acc(1.2);
        acc(2.3);
        acc(3.4);
        acc(4.5);
        acc(0.4);
        acc(2.2);
        acc(7.1);
        acc(4.0);
        BOOST_CHECK_CLOSE(rolling_variance(acc),8.16250000000000,1e-10);
        boost::archive::text_oarchive oa(ss);
        acc.serialize(oa, 0);
    }
    accumulator_set_type other_acc = acc;
    boost::archive::text_iarchive ia(ss);
    other_acc.serialize(ia, 0);
    BOOST_CHECK_CLOSE(rolling_variance(acc),8.16250000000000,1e-10);

}

///////////////////////////////////////////////////////////////////////////////
// test_persistency
//
void test_persistency()
{
    // tag::rolling_window::window_size
    accumulator_set<double, stats<tag::immediate_rolling_variance> >
        acc_immediate_rolling_variance(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::immediate_rolling_variance, tag::rolling_mean> >
        acc_immediate_rolling_variance2(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance(immediate)> >
        acc_immediate_rolling_variance3(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::lazy_rolling_variance> >
        acc_lazy_rolling_variance(tag::lazy_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance(lazy)> >
       acc_lazy_rolling_variance2(tag::immediate_rolling_variance::window_size = window_size);

    accumulator_set<double, stats<tag::rolling_variance> >
        acc_default_rolling_variance(tag::rolling_variance::window_size = window_size);

    //// test the different implementations
    test_persistency_impl(acc_immediate_rolling_variance);
    test_persistency_impl(acc_immediate_rolling_variance2);
    test_persistency_impl(acc_immediate_rolling_variance3);
    test_persistency_impl(acc_lazy_rolling_variance);
    test_persistency_impl(acc_lazy_rolling_variance2);
    test_persistency_impl(acc_default_rolling_variance);
}

///////////////////////////////////////////////////////////////////////////////
// init_unit_test_suite
//
test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite *test = BOOST_TEST_SUITE("rolling variance test");

    test->add(BOOST_TEST_CASE(&test_rolling_variance));
    test->add(BOOST_TEST_CASE(&test_persistency));

    return test;
}
