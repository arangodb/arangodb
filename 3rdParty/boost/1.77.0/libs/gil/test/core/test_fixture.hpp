//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_CORE_TEST_FIXTURE_HPP
#define BOOST_GIL_TEST_CORE_TEST_FIXTURE_HPP

#include <boost/gil/promote_integral.hpp>
#include <boost/assert.hpp>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <random>
#include <tuple>
#include <type_traits>

namespace boost { namespace gil { namespace test { namespace fixture {

template <typename T>
struct consecutive_value
{
    consecutive_value(T start) : current_(start)
    {
        BOOST_ASSERT(static_cast<int>(current_) >= 0);
    }

    T operator()()
    {
        BOOST_ASSERT(static_cast<int>(current_) + 1 > 0);
        current_++;
        return current_;
    }

    T current_;
};

template <typename T>
struct reverse_consecutive_value
{
    reverse_consecutive_value(T start) : current_(start)
    {
        BOOST_ASSERT(static_cast<int>(current_) > 0);
    }

    T operator()()
    {
        BOOST_ASSERT(static_cast<int>(current_) + 1 >= 0);
        current_--;
        return current_;
    }

    T current_;
};

template <typename T>
struct random_value
{
    static_assert(std::is_integral<T>::value, "T must be integral type");
    static constexpr auto range_min = std::numeric_limits<T>::min();
    static constexpr auto range_max = std::numeric_limits<T>::max();

    random_value() : rng_(rd_()), uid_(range_min, range_max) {}

    T operator()()
    {
        auto value = uid_(rng_);
        BOOST_ASSERT(range_min <= value && value <= range_max);
        return static_cast<T>(value);
    }

    std::random_device rd_;
    std::mt19937 rng_;
    std::uniform_int_distribution<typename gil::promote_integral<T>::type> uid_;
};

}}}} // namespace boost::gil::test::fixture

#endif
