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

#include <vector>

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

BOOST_AUTO_TEST_CASE(discrete_distribution)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    size_t size = 100;
    boost::compute::vector<uint_> vec(size, context);

    // initialize the default random engine
    boost::compute::default_random_engine engine(queue);

    // initialize weights
    int weights[] = {10, 40, 40, 10};

    // setup the discrete distribution
    boost::compute::discrete_distribution<uint_> distribution(
        weights, weights + 4
    );

    std::vector<double> p = distribution.probabilities();
    BOOST_CHECK_CLOSE(p[0], double(0.1), 0.001);
    BOOST_CHECK_CLOSE(p[1], double(0.4), 0.001);
    BOOST_CHECK_CLOSE(p[2], double(0.4), 0.001);
    BOOST_CHECK_CLOSE(p[3], double(0.1), 0.001);

    BOOST_CHECK_EQUAL((distribution.min)(), uint_(0));
    BOOST_CHECK_EQUAL((distribution.max)(), uint_(3));

    // generate the random values and store them to 'vec'
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 < 4, queue
        ),
        size
    );
}

BOOST_AUTO_TEST_CASE(discrete_distribution_default_ctor)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    size_t size = 100;
    boost::compute::vector<uint_> vec(size, context);

    // initialize the default random engine
    boost::compute::default_random_engine engine(queue);

    // call default constructor
    boost::compute::discrete_distribution<uint_> distribution;

    std::vector<double> p = distribution.probabilities();
    BOOST_CHECK_CLOSE(p[0], double(1), 0.001);

    // generate the random values and store them to 'vec'
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 == 0, queue
        ),
        size
    );
}

BOOST_AUTO_TEST_CASE(discrete_distribution_one_weight)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    size_t size = 100;
    boost::compute::vector<uint_> vec(size, context);

    // initialize the default random engine
    boost::compute::default_random_engine engine(queue);

    std::vector<int> weights(1, 1);
    // call default constructor
    boost::compute::discrete_distribution<uint_> distribution(
        weights.begin(), weights.end()
    );

    std::vector<double> p = distribution.probabilities();
    BOOST_CHECK_CLOSE(p[0], double(1), 0.001);

    BOOST_CHECK_EQUAL((distribution.min)(), uint_(0));
    BOOST_CHECK_EQUAL((distribution.max)(), uint_(0));

    // generate the random values and store them to 'vec'
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 == 0, queue
        ),
        size
    );
}

BOOST_AUTO_TEST_CASE(discrete_distribution_empty_weights)
{
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    size_t size = 100;
    boost::compute::vector<uint_> vec(size, context);

    // initialize the default random engine
    boost::compute::default_random_engine engine(queue);

    std::vector<int> weights;
    // weights.begin() == weights.end()
    boost::compute::discrete_distribution<uint_> distribution(
        weights.begin(), weights.end()
    );

    std::vector<double> p = distribution.probabilities();
    BOOST_CHECK_CLOSE(p[0], double(1), 0.001);

    BOOST_CHECK_EQUAL((distribution.min)(), uint_(0));
    BOOST_CHECK_EQUAL((distribution.max)(), uint_(0));

    // generate the random values and store them to 'vec'
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 == 0, queue
        ),
        size
    );
}

BOOST_AUTO_TEST_CASE(discrete_distribution_uchar)
{
    using boost::compute::uchar_;
    using boost::compute::uint_;
    using boost::compute::lambda::_1;

    size_t size = 100;
    boost::compute::vector<uchar_> uchar_vec(size, context);
    boost::compute::vector<uint_> uint_vec(size, context);

    // initialize the default random engine
    boost::compute::default_random_engine engine(queue);

    // initialize weights
    std::vector<int> weights(258, 0);
    weights[257] = 1;

    // setup the discrete distribution
    boost::compute::discrete_distribution<uchar_> distribution(
        weights.begin(), weights.end()
    );

    BOOST_CHECK_EQUAL((distribution.min)(), uchar_(0));
    BOOST_CHECK_EQUAL((distribution.max)(), uchar_(255));

    // generate the random uchar_ values to the uchar_ vector
    distribution.generate(uchar_vec.begin(), uchar_vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            uchar_vec.begin(), uchar_vec.end(), _1 == uchar_(1), queue
        ),
        size
    );

    // generate the random uchar_ values to the uint_ vector
    distribution.generate(uint_vec.begin(), uint_vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            uint_vec.begin(), uint_vec.end(), _1 == uint_(1), queue
        ),
        size
    );
}

BOOST_AUTO_TEST_SUITE_END()
