//  (C) Copyright Eric Niebler 2005.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/weighted_mean.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>
#include <sstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace boost;
using namespace unit_test;
using namespace accumulators;

template<typename T>
void assert_is_double(T const &)
{
    BOOST_MPL_ASSERT((is_same<T, double>));
}

typedef accumulator_set<int, stats<tag::mean, 
        tag::mean_of_variates<int, tag::covariate1> > > mean_t;

typedef accumulator_set<int, stats<tag::mean(immediate), 
        tag::mean_of_variates<int, tag::covariate1>(immediate) > > immediate_mean_t;

///////////////////////////////////////////////////////////////////////////////
// test_stat
//
void test_stat()
{
    mean_t acc, test_acc(sample = 0);

    acc(1, covariate1 = 3);
    BOOST_CHECK_CLOSE(1., mean(acc), 1e-5);
    BOOST_CHECK_EQUAL(1u, count(acc));
    BOOST_CHECK_EQUAL(1, sum(acc));
    BOOST_CHECK_CLOSE(3., (accumulators::mean_of_variates<int, tag::covariate1>(acc)), 1e-5);

    acc(0, covariate1 = 4);
    BOOST_CHECK_CLOSE(0.5, mean(acc), 1e-5);
    BOOST_CHECK_EQUAL(2u, count(acc));
    BOOST_CHECK_EQUAL(1, sum(acc));
    BOOST_CHECK_CLOSE(3.5, (accumulators::mean_of_variates<int, tag::covariate1>(acc)), 1e-5);

    acc(2, covariate1 = 8);
    BOOST_CHECK_CLOSE(1., mean(acc), 1e-5);
    BOOST_CHECK_EQUAL(3u, count(acc));
    BOOST_CHECK_EQUAL(3, sum(acc));
    BOOST_CHECK_CLOSE(5., (accumulators::mean_of_variates<int, tag::covariate1>(acc)), 1e-5);

    assert_is_double(mean(acc));

    immediate_mean_t acc2, test_acc2(sample = 0);

    acc2(1, covariate1 = 3);
    BOOST_CHECK_CLOSE(1., mean(acc2), 1e-5);
    BOOST_CHECK_EQUAL(1u, count(acc2));
    BOOST_CHECK_CLOSE(3., (accumulators::mean_of_variates<int, tag::covariate1>(acc2)), 1e-5);

    acc2(0, covariate1 = 4);
    BOOST_CHECK_CLOSE(0.5, mean(acc2), 1e-5);
    BOOST_CHECK_EQUAL(2u, count(acc2));
    BOOST_CHECK_CLOSE(3.5, (accumulators::mean_of_variates<int, tag::covariate1>(acc2)), 1e-5);

    acc2(2, covariate1 = 8);
    BOOST_CHECK_CLOSE(1., mean(acc2), 1e-5);
    BOOST_CHECK_EQUAL(3u, count(acc2));
    BOOST_CHECK_CLOSE(5., (accumulators::mean_of_variates<int, tag::covariate1>(acc2)), 1e-5);

    assert_is_double(mean(acc2));
}

///////////////////////////////////////////////////////////////////////////////
// test_persistency
//
void test_persistency()
{
    std::stringstream ss;
    {
        mean_t acc1;
        immediate_mean_t acc2;

        acc1(1, covariate1 = 3);
        acc1(0, covariate1 = 4);
        acc1(2, covariate1 = 8);
        acc2(1, covariate1 = 3);
        acc2(0, covariate1 = 4);
        acc2(2, covariate1 = 8);
        BOOST_CHECK_CLOSE(1., mean(acc1), 1e-5);
        BOOST_CHECK_CLOSE(5., (accumulators::mean_of_variates<int, tag::covariate1>(acc1)), 1e-5);
        BOOST_CHECK_CLOSE(1., mean(acc2), 1e-5);
        BOOST_CHECK_CLOSE(5., (accumulators::mean_of_variates<int, tag::covariate1>(acc2)), 1e-5);
        boost::archive::text_oarchive oa(ss);
        acc1.serialize(oa, 0);
        acc2.serialize(oa, 0);
    }
    mean_t acc1;
    immediate_mean_t acc2;
    boost::archive::text_iarchive ia(ss);
    acc1.serialize(ia, 0);
    acc2.serialize(ia, 0);
    BOOST_CHECK_CLOSE(1., mean(acc1), 1e-5);
    BOOST_CHECK_CLOSE(5., (accumulators::mean_of_variates<int, tag::covariate1>(acc2)), 1e-5);
}

///////////////////////////////////////////////////////////////////////////////
// init_unit_test_suite
//
test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite *test = BOOST_TEST_SUITE("mean test");

    test->add(BOOST_TEST_CASE(&test_stat));
    test->add(BOOST_TEST_CASE(&test_persistency));

    return test;
}
