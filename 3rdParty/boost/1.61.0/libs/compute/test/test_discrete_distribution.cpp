//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestDiscreteDistribution
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/discrete_distribution.hpp>
#include <boost/compute/lambda.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(discrete_distribution_doctest)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    boost::compute::vector<uint_> vec(100, context);

//! [generate]
// initialize the default random engine
boost::compute::default_random_engine engine(queue);

// initialize weights
int weights[] = {2, 2};

// setup the discrete distribution to produce integers 0 and 1
// with equal weights
boost::compute::discrete_distribution<uint_> distribution(weights, weights+2);

// generate the random values and store them to 'vec'
distribution.generate(vec.begin(), vec.end(), engine, queue);
// ! [generate]

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 > 1, queue
        ),
        size_t(0)
    );
}

BOOST_AUTO_TEST_SUITE_END()
