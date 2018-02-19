//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestBernoulliDistribution
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/bernoulli_distribution.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(bernoulli_distribution_doctest)
{
    boost::compute::vector<bool> vec(10, context);

//! [generate]
// initialize the default random engine
boost::compute::default_random_engine engine(queue);

// setup the bernoulli distribution to produce booleans
// with parameter p = 0.25
boost::compute::bernoulli_distribution<float> distribution(0.25f);

// generate the random values and store them to 'vec'
distribution.generate(vec.begin(), vec.end(), engine, queue);
//! [generate]
}

BOOST_AUTO_TEST_SUITE_END()
