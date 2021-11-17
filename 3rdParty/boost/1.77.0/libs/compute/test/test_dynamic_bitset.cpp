//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestDynamicBitset
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/dynamic_bitset.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(set_and_test)
{
    compute::dynamic_bitset<> bits(1024, queue);

    bits.set(1, queue);
    BOOST_CHECK(bits.test(1, queue) == true);
    BOOST_CHECK(bits.test(2, queue) == false);

    bits.set(1, false, queue);
    BOOST_CHECK(bits.test(1, queue) == false);
    BOOST_CHECK(bits.test(2, queue) == false);
}

BOOST_AUTO_TEST_CASE(count)
{
    compute::dynamic_bitset<> bits(1024, queue);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(0));

    bits.set(1, queue);
    bits.set(8, queue);
    bits.set(129, queue);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(3));

    bits.set(8, false, queue);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(2));

    bits.reset(queue);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(0));
}

BOOST_AUTO_TEST_CASE(resize)
{
    compute::dynamic_bitset<> bits(0, queue);
    BOOST_CHECK_EQUAL(bits.size(), size_t(0));
    BOOST_CHECK_EQUAL(bits.empty(), true);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(0));

    bits.resize(100, queue);
    BOOST_CHECK_EQUAL(bits.size(), size_t(100));
    BOOST_CHECK_EQUAL(bits.empty(), false);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(0));

    bits.set(42, true, queue);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(1));

    bits.resize(0, queue);
    BOOST_CHECK_EQUAL(bits.size(), size_t(0));
    BOOST_CHECK_EQUAL(bits.empty(), true);
    BOOST_CHECK_EQUAL(bits.count(queue), size_t(0));
}

BOOST_AUTO_TEST_CASE(none_and_any)
{
    compute::dynamic_bitset<> bits(1024, queue);
    BOOST_CHECK(bits.any(queue) == false);
    BOOST_CHECK(bits.none(queue) == true);

    bits.set(1023, queue);
    BOOST_CHECK(bits.any(queue) == true);
    BOOST_CHECK(bits.none(queue) == false);

    bits.set(1023, false, queue);
    BOOST_CHECK(bits.any(queue) == false);
    BOOST_CHECK(bits.none(queue) == true);

    bits.set(1, queue);
    BOOST_CHECK(bits.any(queue) == true);
    BOOST_CHECK(bits.none(queue) == false);

    bits.reset(queue);
    BOOST_CHECK(bits.any(queue) == false);
    BOOST_CHECK(bits.none(queue) == true);
}

BOOST_AUTO_TEST_SUITE_END()
