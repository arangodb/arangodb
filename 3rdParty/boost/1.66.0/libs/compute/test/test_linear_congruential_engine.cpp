//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestLinearCongruentialEngine
#include <boost/test/unit_test.hpp>

#include <boost/compute/random/linear_congruential_engine.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(generate_uint)
{
    using boost::compute::uint_;

    boost::compute::linear_congruential_engine<uint_> rng(queue);

    boost::compute::vector<uint_> vector(10, context);

    rng.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(1099087573),
         uint_(2291457337),
         uint_(4026424941),
         uint_(420705969),
         uint_(2250972997),
         uint_(153107049),
         uint_(3581708125),
         uint_(1733142113),
         uint_(3008982197),
         uint_(3237988505))
    );
}

BOOST_AUTO_TEST_CASE(discard_uint)
{
    using boost::compute::uint_;

    boost::compute::linear_congruential_engine<uint_> rng(queue);

    boost::compute::vector<uint_> vector(5, context);

    rng.discard(5, queue);
    rng.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 5, vector,
        (uint_(153107049),
         uint_(3581708125),
         uint_(1733142113),
         uint_(3008982197),
         uint_(3237988505))
    );
}

BOOST_AUTO_TEST_CASE(copy_ctor)
{
    using boost::compute::uint_;

    boost::compute::linear_congruential_engine<uint_> rng(queue);
    boost::compute::linear_congruential_engine<uint_> rng_copy(rng);

    boost::compute::vector<uint_> vector(10, context);

    rng_copy.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(1099087573),
         uint_(2291457337),
         uint_(4026424941),
         uint_(420705969),
         uint_(2250972997),
         uint_(153107049),
         uint_(3581708125),
         uint_(1733142113),
         uint_(3008982197),
         uint_(3237988505))
    );
}

BOOST_AUTO_TEST_CASE(assign_op)
{
    using boost::compute::uint_;

    boost::compute::linear_congruential_engine<uint_> rng(queue);
    boost::compute::linear_congruential_engine<uint_> rng_copy(queue);

    boost::compute::vector<uint_> vector(10, context);

    rng_copy.discard(5, queue);
    rng_copy = rng;
    rng_copy.generate(vector.begin(), vector.end(), queue);

    CHECK_RANGE_EQUAL(
        uint_, 10, vector,
        (uint_(1099087573),
         uint_(2291457337),
         uint_(4026424941),
         uint_(420705969),
         uint_(2250972997),
         uint_(153107049),
         uint_(3581708125),
         uint_(1733142113),
         uint_(3008982197),
         uint_(3237988505))
    );
}

BOOST_AUTO_TEST_SUITE_END()
