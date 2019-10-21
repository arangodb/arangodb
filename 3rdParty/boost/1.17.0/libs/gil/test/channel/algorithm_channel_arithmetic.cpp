//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel_algorithm.hpp>
#include <type_traits>
#include <utility>

#define BOOST_TEST_MODULE test_algorithm_channel_arithmetic
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

template <typename ChannelFixtureBase>
void test_channel_arithmetic_mutable(boost::mpl::false_)  {}

template <typename ChannelFixtureBase>
void test_channel_arithmetic_mutable(boost::mpl::true_)
{
    using fixture_t = fixture::channel<ChannelFixtureBase>;
    using channel_value_t = typename fixture_t::channel_value_t;
    fixture_t f;
    channel_value_t const v = f.min_v_;
    channel_value_t const one = 1;

    ++f.min_v_;
    f.min_v_++;
    --f.min_v_;
    f.min_v_--;
    BOOST_TEST(v == f.min_v_);

    f.min_v_ += one;
    f.min_v_ -= one;
    BOOST_TEST(v == f.min_v_);

    f.min_v_ *= one;
    f.min_v_ /= one;
    BOOST_TEST(v == f.min_v_);

    f.min_v_ = one; // assignable to scalar
    BOOST_TEST(f.min_v_ == one);
    f.min_v_ = v; // and to value type
    BOOST_TEST(f.min_v_ == v);

    // test swap
    channel_value_t v1 = f.min_v_;
    channel_value_t v2 = f.max_v_;
    std::swap(f.min_v_, f.max_v_);
    BOOST_TEST(f.min_v_ > f.max_v_);
    channel_value_t v3 = f.min_v_;
    channel_value_t v4 = f.max_v_;
    BOOST_TEST(v1 == v4);
    BOOST_TEST(v2 == v3);
}

template <typename ChannelFixtureBase>
void test_channel_arithmetic()
{
    using fixture_t = fixture::channel<ChannelFixtureBase>;
    fixture_t f;
    BOOST_TEST(f.min_v_ * 1 == f.min_v_);
    BOOST_TEST(f.min_v_ / 1 == f.min_v_);
    BOOST_TEST((f.min_v_ + 1) + 1 == f.min_v_ + 2);
    BOOST_TEST((f.max_v_ - 1) - 1 == f.max_v_ - 2);

    using is_mutable_t = boost::mpl::bool_
        <
        gil::channel_traits<typename fixture_t::channel_t>::is_mutable
        >;
    test_channel_arithmetic_mutable<ChannelFixtureBase>(is_mutable_t());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_value, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_value<Channel>;
    test_channel_arithmetic<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(channel_reference, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_reference<Channel&>;
    test_channel_arithmetic<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    channel_reference_const, Channel, fixture::channel_byte_types)
{
    using fixture_t = fixture::channel_reference<Channel const&>;
    test_channel_arithmetic<fixture_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_channel_reference, BitField, fixture::channel_bitfield_types)
{
    using channels565_t = fixture::packed_channels565<BitField>;
    test_channel_arithmetic<typename channels565_t::fixture_0_5_t>();
    test_channel_arithmetic<typename channels565_t::fixture_5_6_t >();
    test_channel_arithmetic<typename channels565_t::fixture_11_5_t>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    packed_dynamic_channel_reference, BitField, fixture::channel_bitfield_types)
{
    using channels565_t = fixture::packed_dynamic_channels565<BitField>;
    test_channel_arithmetic<typename channels565_t::fixture_5_t>();
    test_channel_arithmetic<typename channels565_t::fixture_6_t>();
}
