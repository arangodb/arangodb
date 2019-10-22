//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel_algorithm.hpp>

#define BOOST_TEST_MODULE test_algorithm_channel_multiply
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

template <typename ChannelFixtureBase>
void test_channel_multiply()
{
    fixture::channel<ChannelFixtureBase> f;
    BOOST_TEST(gil::channel_multiply(f.min_v_, f.min_v_) == f.min_v_);
    BOOST_TEST(gil::channel_multiply(f.max_v_, f.max_v_) == f.max_v_);
    BOOST_TEST(gil::channel_multiply(f.max_v_, f.min_v_) == f.min_v_);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_value<Channel>;
    test_channel_multiply<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_reference<Channel&>;
    test_channel_multiply<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    channel_reference_const, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_reference<Channel const&>;
    test_channel_multiply<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_channel_reference, BitField, fixture::channel_bitfield_types)
{
    using channels565_t = fixture::packed_channels565<BitField>;
    test_channel_multiply<typename channels565_t::fixture_0_5_t>();
    test_channel_multiply<typename channels565_t::fixture_5_6_t >();
    test_channel_multiply<typename channels565_t::fixture_11_5_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_dynamic_channel_reference, BitField, fixture::channel_bitfield_types)
{
    using channels565_t = fixture::packed_dynamic_channels565<BitField>;
    test_channel_multiply<typename channels565_t::fixture_5_t>();
    test_channel_multiply<typename channels565_t::fixture_6_t>();
}
