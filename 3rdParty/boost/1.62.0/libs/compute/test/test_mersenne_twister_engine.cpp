//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestMersenneTwisterEngine
#include <boost/test/unit_test.hpp>

#include <boost/compute/random/mersenne_twister_engine.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(generate_uint)
{
    using boost::compute::uint_;

    boost::compute::mt19937 rng(queue);

    boost::compute::vector<uint_> vector(10, context);

    rng.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(3499211612),
         uint_(581869302),
         uint_(3890346734),
         uint_(3586334585),
         uint_(545404204),
         uint_(4161255391),
         uint_(3922919429),
         uint_(949333985),
         uint_(2715962298),
         uint_(1323567403))
    );
}

BOOST_AUTO_TEST_CASE(discard_uint)
{
    using boost::compute::uint_;

    boost::compute::mt19937 rng(queue);

    boost::compute::vector<uint_> vector(5, context);

    rng.discard(5, queue);
    rng.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 5, vector,
        (uint_(4161255391),
         uint_(3922919429),
         uint_(949333985),
         uint_(2715962298),
         uint_(1323567403))
    );
}

BOOST_AUTO_TEST_CASE(copy_ctor)
{
    using boost::compute::uint_;

    boost::compute::mt19937 rng(queue);
    boost::compute::mt19937 rng_copy(rng);

    boost::compute::vector<uint_> vector(10, context);

    rng_copy.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(3499211612),
         uint_(581869302),
         uint_(3890346734),
         uint_(3586334585),
         uint_(545404204),
         uint_(4161255391),
         uint_(3922919429),
         uint_(949333985),
         uint_(2715962298),
         uint_(1323567403))
    );
}

BOOST_AUTO_TEST_CASE(assign_op)
{
    using boost::compute::uint_;

    boost::compute::mt19937 rng(queue);
    boost::compute::mt19937 rng_copy(queue);

    boost::compute::vector<uint_> vector(10, context);

    rng_copy.discard(5, queue);
    rng_copy = rng;
    rng_copy.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(3499211612),
         uint_(581869302),
         uint_(3890346734),
         uint_(3586334585),
         uint_(545404204),
         uint_(4161255391),
         uint_(3922919429),
         uint_(949333985),
         uint_(2715962298),
         uint_(1323567403))
    );
}

BOOST_AUTO_TEST_SUITE_END()
