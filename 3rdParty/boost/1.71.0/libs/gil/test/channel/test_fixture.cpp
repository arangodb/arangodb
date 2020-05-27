//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <limits>

#define BOOST_TEST_MODULE test_channel_test_fixture
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace fixture = boost::gil::test::fixture;

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_minmax_value_integral, Channel, fixture::channel_integer_types)
{
    fixture::channel_minmax_value<Channel> fix;
    fixture::channel_minmax_value<Channel> exp;
    BOOST_TEST(fix.min_v_ == exp.min_v_);
    BOOST_TEST(fix.max_v_ == exp.max_v_);
    BOOST_TEST(fix.min_v_ == std::numeric_limits<Channel>::min());
    BOOST_TEST(fix.max_v_ == std::numeric_limits<Channel>::max());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_minmax_value_float, Channel, fixture::channel_float_types)
{
    fixture::channel_minmax_value<Channel> fix;
    fixture::channel_minmax_value<Channel> exp;
    BOOST_TEST(fix.min_v_ == exp.min_v_);
    BOOST_TEST(fix.max_v_ == exp.max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_byte_types)
{
    fixture::channel_value<Channel> fix;
    fixture::channel_minmax_value<Channel> exp;
    BOOST_TEST(fix.min_v_ == exp.min_v_);
    BOOST_TEST(fix.max_v_ == exp.max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_byte_types)
{
    fixture::channel_reference<Channel&> fix;
    fixture::channel_minmax_value<Channel> exp;
    BOOST_TEST(fix.min_v_ == exp.min_v_);
    BOOST_TEST(fix.max_v_ == exp.max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    channel_reference_const, Channel, fixture::channel_byte_types)
{
    fixture::channel_reference<Channel const&> fix;
    fixture::channel_minmax_value<Channel> exp;
    BOOST_TEST(fix.min_v_ == exp.min_v_);
    BOOST_TEST(fix.max_v_ == exp.max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_channels565, BitField, fixture::channel_bitfield_types)
{
    static_assert(std::is_integral<BitField>::value, "bitfield is not integral type");

    // Regardless of BitField buffer bit-size, the fixture is initialized
    // with max value that fits into 5+6+5 bit integer
    fixture::packed_channels565<BitField> fix;
    fixture::channel_minmax_value<std::uint16_t> exp;
    BOOST_TEST(fix.data_ == exp.max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_dynamic_channels565, BitField, fixture::channel_bitfield_types)
{
    static_assert(std::is_integral<BitField>::value, "bitfield is not integral type");

    // Regardless of BitField buffer bit-size, the fixture is initialized
    // with max value that fits into 5+6+5 bit integer
    fixture::packed_dynamic_channels565<BitField> fix;
    fixture::channel_minmax_value<std::uint16_t> exp;
    BOOST_TEST(fix.data_ == exp.max_v_);
}
