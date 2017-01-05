//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUniformIntDistribution
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/uniform_int_distribution.hpp>
#include <boost/compute/lambda.hpp>

#include "context_setup.hpp"

namespace compute=boost::compute;

BOOST_AUTO_TEST_CASE(uniform_int_distribution_doctest)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    boost::compute::vector<uint_> vec(128, context);

//! [generate]
// initialize the default random engine
boost::compute::default_random_engine engine(queue);

// setup the uniform distribution to produce integers 0 and 1
boost::compute::uniform_int_distribution<uint_> distribution(0, 1);

// generate the random values and store them to 'vec'
distribution.generate(vec.begin(), vec.end(), engine, queue);
//! [generate]

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 > 1, queue
        ),
        size_t(0)
    );
}

BOOST_AUTO_TEST_CASE(issue159) {
    using boost::compute::lambda::_1;

    boost::compute::vector<int> input(10, context);

    // generate random numbers between 1 and 10
    compute::default_random_engine rng(queue);
    compute::uniform_int_distribution<int> d(1, 10);
    d.generate(input.begin(), input.end(), rng, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            input.begin(), input.end(), _1 > 10, queue
        ),
        size_t(0)
    );

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            input.begin(), input.end(), _1 < 1, queue
        ),
        size_t(0)
    );
}

BOOST_AUTO_TEST_SUITE_END()
