//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel_algorithm.hpp>
#include <cstdint>

#define BOOST_TEST_MODULE test_algorithm_channel_convert
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

template <typename ChannelFixtureBase>
struct test_convert_to
{
    using channel_t = typename fixture::channel<ChannelFixtureBase>::channel_t;
    using channel_value_t = typename fixture::channel<ChannelFixtureBase>::channel_value_t;

    template <typename Channel>
    static void from(Channel src_min_v, Channel src_max_v)
    {
        channel_value_t min_v = gil::channel_convert<channel_t>(src_min_v);
        channel_value_t max_v = gil::channel_convert<channel_t>(src_max_v);
        fixture::channel_minmax_value<channel_value_t> expect;
        BOOST_TEST(min_v == expect.min_v_);
        BOOST_TEST(max_v == expect.max_v_);
    }
};

//--- Test gil::channel_convert from integral channels to all byte channels -------------
#define GIL_TEST_CHANNEL_CONVERT_FROM(source_channel_type) \
    BOOST_FIXTURE_TEST_SUITE( \
        channel_convert_from_##source_channel_type, \
        fixture::channel_minmax_value<std::source_channel_type>) \
    BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_byte_types) \
    {   test_convert_to<fixture::channel_value<Channel>>::from(min_v_, max_v_); } \
    BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_byte_types) \
    {   test_convert_to<fixture::channel_reference<Channel&>>::from(min_v_, max_v_); } \
    BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference_const, Channel, fixture::channel_byte_types) \
    {   test_convert_to<fixture::channel_reference<Channel const&>>::from(min_v_, max_v_); } \
    BOOST_AUTO_TEST_CASE(packed_channel_reference) \
    { \
        using channels565_t = fixture::packed_channels565<std::uint16_t>; \
        test_convert_to<typename channels565_t::fixture_0_5_t>::from(min_v_, max_v_); \
        test_convert_to<typename channels565_t::fixture_5_6_t>::from(min_v_, max_v_); \
        test_convert_to<typename channels565_t::fixture_11_5_t>::from(min_v_, max_v_); \
    } \
    BOOST_AUTO_TEST_CASE(packed_dynamic_channel_reference) \
    { \
        using channels565_t = fixture::packed_dynamic_channels565<std::uint16_t>; \
        test_convert_to<typename channels565_t::fixture_5_t>::from(min_v_, max_v_); \
        test_convert_to<typename channels565_t::fixture_6_t>::from(min_v_, max_v_); \
    } \
    BOOST_AUTO_TEST_SUITE_END()

GIL_TEST_CHANNEL_CONVERT_FROM(uint8_t)
GIL_TEST_CHANNEL_CONVERT_FROM(int8_t)
GIL_TEST_CHANNEL_CONVERT_FROM(uint16_t)
GIL_TEST_CHANNEL_CONVERT_FROM(int16_t)
GIL_TEST_CHANNEL_CONVERT_FROM(uint32_t)
GIL_TEST_CHANNEL_CONVERT_FROM(int32_t)

#undef GIL_TEST_CHANNEL_CONVERT_FROM

// FIXME: gil::float32_t <-> gil::float64_t seems not supported

//--- Test gil::channel_convert from gil::float32_t to all integer channels -------------
BOOST_FIXTURE_TEST_SUITE(channel_convert_from_float32_t,
                         fixture::channel_minmax_value<gil::float32_t>)

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_value<Channel>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_reference<Channel&>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    channel_reference_const, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_reference<Channel const&>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_SUITE_END()

//--- Test gil::channel_convert from gil::float64_t to all integer channels -------------
BOOST_FIXTURE_TEST_SUITE(channel_convert_from_float64_t,
                         fixture::channel_minmax_value<gil::float64_t>)

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_value<Channel>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_reference<Channel&>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    channel_reference_const, Channel, fixture::channel_integer_types)
{
    test_convert_to<fixture::channel_reference<Channel const&>>::from(min_v_, max_v_);
}

BOOST_AUTO_TEST_SUITE_END()
