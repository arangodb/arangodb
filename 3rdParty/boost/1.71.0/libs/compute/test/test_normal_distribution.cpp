//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestNormalDistribution
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/normal_distribution.hpp>
#include <boost/compute/lambda.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "context_setup.hpp"

template<class Stats, class T>
boost::accumulators::accumulator_set<T, Stats>
accumulate_statistics(const boost::compute::vector<T>& vector,
                      boost::compute::command_queue& queue) {
    // copy vector to the host
    std::vector<T> host_vector(vector.size());
    boost::compute::copy(
        vector.begin(), vector.end(), host_vector.begin(), queue
    );

    // compute desired statistics and return accumulator object
    return std::for_each(
        host_vector.begin(),
        host_vector.end(),
        boost::accumulators::accumulator_set<T, Stats>()
    );
}

BOOST_AUTO_TEST_CASE(normal_distribution_doctest)
{
    using boost::compute::lambda::_1;

    boost::compute::vector<float> vec(10, context);

//! [generate]
// initialize the default random engine
boost::compute::default_random_engine engine(queue);

// setup the normal distribution to produce floats centered at 5
boost::compute::normal_distribution<float> distribution(5.0f, 1.0f);

// generate the random values and store them to 'vec'
distribution.generate(vec.begin(), vec.end(), engine, queue);
//! [generate]
}

BOOST_AUTO_TEST_CASE(normal_distribution_statistics)
{
    // generate normally distributed random numbers
    const size_t n = 10000;
    boost::compute::vector<float> vec(n, context);
    boost::compute::default_random_engine engine(queue);
    boost::compute::normal_distribution<float> distribution(10.0f, 2.0f);
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    // compute mean and standard deviation
    using namespace boost::accumulators;
    accumulator_set<float, stats<tag::variance> > acc =
        accumulate_statistics<stats<tag::variance> >(vec, queue);

    // check mean and standard deviation are what we expect
    BOOST_CHECK_CLOSE(mean(acc), 10.f, 0.5f);
    BOOST_CHECK_CLOSE(std::sqrt(variance(acc)), 2.f, 0.5f);
}

BOOST_AUTO_TEST_SUITE_END()
