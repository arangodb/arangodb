//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/config.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include <boost/gil/concepts.hpp>
#include <cstdint>

#define BOOST_TEST_MODULE test_channel_concepts
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

// A channel archetype - to test the minimum requirements of the concept
struct channel_value_archetype;

struct channel_archetype
{
    // equality comparable
    friend bool operator==(channel_archetype const&, channel_archetype const&)
    { return true; }
    // inequality comparable
    friend bool operator!=(channel_archetype const&, channel_archetype const&)
    { return false; }
    // less-than comparable
    friend bool operator<(channel_archetype const&, channel_archetype const&)
    { return false; }
    // convertible to a scalar
    operator std::uint8_t() const { return 0; }

    channel_archetype& operator++() { return *this; }
    channel_archetype& operator--() { return *this; }
    channel_archetype  operator++(int) { return *this; }
    channel_archetype  operator--(int) { return *this; }

    template <typename Scalar>
    channel_archetype operator+=(Scalar) { return *this; }
    template <typename Scalar>
    channel_archetype operator-=(Scalar) { return *this; }
    template <typename Scalar>
    channel_archetype operator*=(Scalar) { return *this; }
    template <typename Scalar>
    channel_archetype operator/=(Scalar) { return *this; }

    using value_type        = channel_value_archetype;
    using reference         = channel_archetype;
    using const_reference   = channel_archetype const;
    using pointer           = channel_value_archetype*;
    using const_pointer     = channel_value_archetype const*;
    static constexpr bool is_mutable = true;

    static value_type min_value();
    static value_type max_value();
};

struct channel_value_archetype : public channel_archetype
{
    // default constructible
    channel_value_archetype() {}
    // copy constructible
    channel_value_archetype(channel_value_archetype const&) = default;
    // assignable
    channel_value_archetype& operator=(channel_value_archetype const&)
    {return *this;}
    channel_value_archetype(std::uint8_t) {}
};

channel_value_archetype channel_archetype::min_value()
{
    return channel_value_archetype();
}

channel_value_archetype channel_archetype::max_value()
{
    return channel_value_archetype();
}

BOOST_AUTO_TEST_CASE(channel_minimal_requirements)
{
    // Do only compile-time tests for the archetype
    // (because asserts like val1<val2 fail)
    boost::function_requires<gil::MutableChannelConcept<channel_archetype>>();

    fixture::channel_value<channel_value_archetype>();
    fixture::channel_reference<channel_archetype>();
    fixture::channel_reference<channel_archetype const&>();
}
