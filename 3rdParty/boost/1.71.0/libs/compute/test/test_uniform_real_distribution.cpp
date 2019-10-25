//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUniformRealDistribution
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/uniform_real_distribution.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(uniform_real_distribution_doctest)
{
    using boost::compute::lambda::_1;

    boost::compute::vector<float> vec(128, context);

//! [generate]
// initialize the default random engine
boost::compute::default_random_engine engine(queue);

// setup the uniform distribution to produce floats between 1 and 100
boost::compute::uniform_real_distribution<float> distribution(1.0f, 100.0f);

// generate the random values and store them to 'vec'
distribution.generate(vec.begin(), vec.end(), engine, queue);
//! [generate]

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 < 1.0f || _1 >= 100.0f, queue
        ),
        size_t(0)
    );
}

template<class T>
class range_test_engine
{
public:
    explicit range_test_engine(boost::compute::command_queue &queue) {
        (void) queue;
    }

    template<class OutputIterator, class Function>
    void generate(OutputIterator first, OutputIterator last, Function op, boost::compute::command_queue &queue)
    {
        boost::compute::vector<T> tmp(std::distance(first, last), queue.get_context());

        BOOST_COMPUTE_FUNCTION(T, max_random, (const T x),
        {
            if(get_global_id(0) < 1)
                return (ValueType) MAX_RANDOM;
            else
                return (ValueType) 0;
        });

        max_random.define("MAX_RANDOM", "UINT_MAX");
        max_random.define("ValueType", boost::compute::type_name<T>());

        boost::compute::transform(tmp.begin(), tmp.end(), tmp.begin(), max_random, queue);
        boost::compute::transform(tmp.begin(), tmp.end(), first, op, queue);
    }
};

// For checking if result is in the correct range [low, hi)
BOOST_AUTO_TEST_CASE(uniform_real_distribution_range)
{
    using boost::compute::lambda::_1;

    boost::compute::vector<float> vec(32, context);

    // initialize the range_test_engine
    range_test_engine<boost::compute::uint_> engine(queue);

    // setup the uniform distribution to produce floats between 0.9 and 1.0
    boost::compute::uniform_real_distribution<float> distribution(0.9f, 1.0f);

    // generate the random values and store them to 'vec'
    distribution.generate(vec.begin(), vec.end(), engine, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::count_if(
            vec.begin(), vec.end(), _1 < 0.9f || _1 >= 1.0f, queue
        ),
        size_t(0)
    );
}

BOOST_AUTO_TEST_SUITE_END()
