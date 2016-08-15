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

#include "context_setup.hpp"

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

BOOST_AUTO_TEST_SUITE_END()
