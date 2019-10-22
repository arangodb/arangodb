//  (C) Copyright Eric Niebler, Olivier Gygi 2006.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Test case for p_square_cumul_dist.hpp

#include <cmath>
#include <boost/random.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/accumulators/numeric/functional/vector.hpp>
#include <boost/accumulators/numeric/functional/complex.hpp>
#include <boost/accumulators/numeric/functional/valarray.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/p_square_cumul_dist.hpp>
#include <sstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace boost;
using namespace unit_test;
using namespace boost::accumulators;

///////////////////////////////////////////////////////////////////////////////
// erf() not known by VC++ compiler!
// my_erf() computes error function by numerically integrating with trapezoidal rule
//
double my_erf(double const& x, int const& n = 1000)
{
    double sum = 0.;
    double delta = x/n;
    for (int i = 1; i < n; ++i)
        sum += std::exp(-i*i*delta*delta) * delta;
    sum += 0.5 * delta * (1. + std::exp(-x*x));
    return sum * 2. / std::sqrt(3.141592653);
}

typedef accumulator_set<double, stats<tag::p_square_cumulative_distribution> > accumulator_t;
typedef iterator_range<std::vector<std::pair<double, double> >::iterator > histogram_type;

///////////////////////////////////////////////////////////////////////////////
// test_stat
//
void test_stat()
{
    // tolerance in %
    double epsilon = 3;

    accumulator_t acc(p_square_cumulative_distribution_num_cells = 100);

    // two random number generators
    boost::lagged_fibonacci607 rng;
    boost::normal_distribution<> mean_sigma(0,1);
    boost::variate_generator<boost::lagged_fibonacci607&, boost::normal_distribution<> > normal(rng, mean_sigma);

    for (std::size_t i=0; i<1000000; ++i)
    {
        acc(normal());
    }

    histogram_type histogram = p_square_cumulative_distribution(acc);

    for (std::size_t i = 0; i < histogram.size(); ++i)
    {
        // problem with small results: epsilon is relative (in percent), not absolute!
        if ( histogram[i].second > 0.001 )
            BOOST_CHECK_CLOSE( 0.5 * (1.0 + my_erf( histogram[i].first / std::sqrt(2.0) )), histogram[i].second, epsilon );
    }
}

///////////////////////////////////////////////////////////////////////////////
// test_persistency
//
void test_persistency()
{
    // "persistent" storage
    std::stringstream ss;
    // tolerance in %
    double epsilon = 3;
    {
        accumulator_t acc(p_square_cumulative_distribution_num_cells = 100);

        // two random number generators
        boost::lagged_fibonacci607 rng;
        boost::normal_distribution<> mean_sigma(0,1);
        boost::variate_generator<boost::lagged_fibonacci607&, boost::normal_distribution<> > normal(rng, mean_sigma);

        for (std::size_t i=0; i<1000000; ++i)
        {
            acc(normal());
        }

        histogram_type histogram = p_square_cumulative_distribution(acc);

        BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[25].first / std::sqrt(2.0))), histogram[25].second, epsilon);
        BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[50].first / std::sqrt(2.0))), histogram[50].second, epsilon);
        BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[75].first / std::sqrt(2.0))), histogram[75].second, epsilon);
        boost::archive::text_oarchive oa(ss);
        acc.serialize(oa, 0);
    }
    accumulator_t acc(p_square_cumulative_distribution_num_cells = 100);
    boost::archive::text_iarchive ia(ss);
    acc.serialize(ia, 0);
    histogram_type histogram = p_square_cumulative_distribution(acc);
    BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[25].first / std::sqrt(2.0))), histogram[25].second, epsilon);
    BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[50].first / std::sqrt(2.0))), histogram[50].second, epsilon);
    BOOST_CHECK_CLOSE(0.5 * (1.0 + my_erf(histogram[75].first / std::sqrt(2.0))), histogram[75].second, epsilon);
}

///////////////////////////////////////////////////////////////////////////////
// init_unit_test_suite
//
test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite *test = BOOST_TEST_SUITE("p_square_cumulative_distribution test");

    test->add(BOOST_TEST_CASE(&test_stat));
    test->add(BOOST_TEST_CASE(&test_persistency));

    return test;
}

