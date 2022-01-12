//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif BOOST_GCC >= 40700
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <boost/gil.hpp>

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

void test_consecutive_value()
{
    fixture::consecutive_value<std::uint8_t> v(10);
    BOOST_TEST_EQ(v(), std::uint8_t{11});
    BOOST_TEST_EQ(v(), std::uint8_t{12});
    BOOST_TEST_EQ(v(), std::uint8_t{13});
}

void test_reverse_consecutive_value()
{
    fixture::reverse_consecutive_value<std::uint8_t> v(10);
    BOOST_TEST_EQ(v(), std::uint8_t{9});
    BOOST_TEST_EQ(v(), std::uint8_t{8});
    BOOST_TEST_EQ(v(), std::uint8_t{7});
}

void test_random_value()
{
    // Generate N pseudo-random values
    fixture::random_value<std::uint8_t> random;
    std::vector<std::uint8_t> v(10, 0);
    for (auto& i : v) i = random();

    // Require not all of N values are equal (duplicates are possible!)
    std::sort(v.begin(), v.end());
    auto last = std::unique(v.begin(), v.end());
    v.erase(last, v.end());
    BOOST_TEST_GT(v.size(), 0);
    BOOST_TEST_LE(v.size(), 10);
}

int main()
{
    test_consecutive_value();
    test_reverse_consecutive_value();
    test_random_value();

    return boost::report_errors();
}
