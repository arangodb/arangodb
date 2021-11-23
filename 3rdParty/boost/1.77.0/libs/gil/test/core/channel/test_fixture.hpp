//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_CORE_CHANNEL_TEST_FIXTURE_HPP
#define BOOST_GIL_TEST_CORE_CHANNEL_TEST_FIXTURE_HPP

#include <boost/gil/channel.hpp>
#include <boost/gil/concepts.hpp>
#include <boost/gil/typedefs.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>

#include "test_utility_output_stream.hpp"

namespace boost { namespace gil { namespace test { namespace fixture {

using channel_byte_types = std::tuple
    <
        std::uint8_t,
        std::int8_t,
        std::uint16_t,
        std::int16_t,
        std::uint32_t,
        std::int32_t,
        gil::float32_t,
        gil::float64_t
    >;

using channel_integer_types = std::tuple
    <
        std::uint8_t,
        std::int8_t,
        std::uint16_t,
        std::int16_t,
        std::uint32_t,
        std::int32_t
    >;

using channel_integer_signed_types = std::tuple
    <
        std::int8_t,
        std::int16_t,
        std::int32_t
    >;

using channel_integer_unsigned_types = std::tuple
    <
        std::uint8_t,
        std::uint16_t,
        std::uint32_t
    >;

// FIXME: If float types are convertible between each other,
// currently they are not, then move to channel_byte_types and
// remove channel_integer_types as redundant.
using channel_float_types = std::tuple
    <
        gil::float32_t,
        gil::float64_t
    >;

using channel_bitfield_types = std::tuple
    <
        std::uint16_t,
        std::uint32_t,
        std::uint64_t
        // TODO: Shall we test signed types for unexpected conversions, etc.?
    >;


template <typename ChannelValue>
struct channel_minmax_value
{
    //static_assert(std::)
    ChannelValue min_v_;
    ChannelValue max_v_;
    channel_minmax_value()
        : min_v_(gil::channel_traits<ChannelValue>::min_value())
        , max_v_(gil::channel_traits<ChannelValue>::max_value())
    {}
};

template <typename ChannelFixtureBase>
struct channel : public ChannelFixtureBase
{
    using channel_t = typename ChannelFixtureBase::channel_t;
    using channel_value_t = typename gil::channel_traits<channel_t>::value_type;

    channel()
    {
        BOOST_TEST_EQ(this->min_v_, gil::channel_traits<channel_t>::min_value());
        BOOST_TEST_EQ(this->max_v_, gil::channel_traits<channel_t>::max_value());
    }
};

// The channel fixtures are defined for different types of channels
// (ie. channel values, references and subbyte references)
// ensure there are two members, min_v_ and max_v_ initialized
// with the minimum and maximum channel value.
// The different channel types have different ways to initialize them,
// thus require different fixtures provided.

// For basic channel types values can be initialized directly.
template <typename ChannelValue>
struct channel_value
{
    using channel_t = ChannelValue;
    channel_t min_v_;
    channel_t max_v_;

    channel_value()
        : min_v_(gil::channel_traits<ChannelValue>::min_value())
        , max_v_(gil::channel_traits<ChannelValue>::max_value())
    {
        boost::function_requires<gil::ChannelValueConcept<ChannelValue>>();
    }
};

// For channel references we need to have separate channel values.
template <typename ChannelRef>
struct channel_reference
    : public channel_value<typename gil::channel_traits<ChannelRef>::value_type>
{
    using parent_t = channel_value<typename gil::channel_traits<ChannelRef>::value_type>;
    using channel_t = ChannelRef;
    channel_t min_v_;
    channel_t max_v_;

    channel_reference()
        : parent_t()
        , min_v_(parent_t::min_v_)
        , max_v_(parent_t::max_v_)
    {
        boost::function_requires<ChannelConcept<ChannelRef>>();
    }
};

// For sub-byte channel references we need to store the bit buffers somewhere
template <typename ChannelSubbyteRef, typename ChannelMutableRef = ChannelSubbyteRef>
struct packed_channel_reference
{
    using channel_t = ChannelSubbyteRef;
    using integer_t = typename channel_t::integer_t;
    channel_t min_v_;
    channel_t max_v_;
    integer_t min_bitbuf_;
    integer_t max_bitbuf_;

    packed_channel_reference() : min_v_(&min_bitbuf_), max_v_(&max_bitbuf_)
    {
        boost::function_requires<ChannelConcept<ChannelSubbyteRef>>();

        ChannelMutableRef b1(&min_bitbuf_);
        b1 = gil::channel_traits<channel_t>::min_value();
        ChannelMutableRef b2(&max_bitbuf_);
        b2 = gil::channel_traits<channel_t>::max_value();
    }
};

// For sub-byte channel references we need to store the bit buffers somewhere
template <typename ChannelSubbyteRef, typename ChannelMutableRef = ChannelSubbyteRef>
struct packed_dynamic_channel_reference
{
    using channel_t = ChannelSubbyteRef;
    using integer_t = typename channel_t::integer_t;
    channel_t min_v_;
    channel_t max_v_;
    integer_t min_bitbuf_;
    integer_t max_bitbuf_;

    packed_dynamic_channel_reference(int first_bit1 = 1, int first_bit2 = 2)
        : min_v_(&min_bitbuf_, first_bit1)
        , max_v_(&max_bitbuf_, first_bit2)
    {
        boost::function_requires<ChannelConcept<ChannelSubbyteRef>>();

        ChannelMutableRef b1(&min_bitbuf_, 1);
        b1 = gil::channel_traits<channel_t>::min_value();
        ChannelMutableRef b2(&max_bitbuf_, 2);
        b2 = gil::channel_traits<channel_t>::max_value();
    }
};

// Concrete fixture for 16-bit pack of 5,6,5-bit channels
template <typename BitField>
struct packed_channels565
{
    static_assert(sizeof(BitField) >= sizeof(std::uint16_t), "16-bit or more required");
    using channel_0_5_t = gil::packed_channel_reference<BitField, 0, 5,true>;
    using channel_5_6_t = gil::packed_channel_reference<BitField, 5, 6,true>;
    using channel_11_5_t = gil::packed_channel_reference<BitField, 11, 5,true>;

    using fixture_0_5_t = fixture::packed_channel_reference<channel_0_5_t>;
    using fixture_5_6_t = fixture::packed_channel_reference<channel_5_6_t>;
    using fixture_11_5_t = fixture::packed_channel_reference<channel_11_5_t>;

    std::uint16_t data_ = 0;
    channel_0_5_t channel1_;
    channel_5_6_t channel2_;
    channel_11_5_t channel3_;

    packed_channels565() : channel1_(&data_), channel2_(&data_), channel3_(&data_)
    {
        channel1_ = gil::channel_traits<channel_0_5_t>::max_value();
        channel2_ = gil::channel_traits<channel_5_6_t>::max_value();
        channel3_ = gil::channel_traits<channel_11_5_t>::max_value();
#ifdef BOOST_TEST_EQ
        BOOST_TEST_EQ(data_, 65535);
#endif
    }
};

// Concrete fixture for dynamically-referenced 16-bit pack of 5,6,5-bit channels
template <typename BitField>
struct packed_dynamic_channels565
{
    static_assert(sizeof(BitField) >= sizeof(std::uint16_t), "16-bit or more required");
    using channel_5_t = gil::packed_dynamic_channel_reference<BitField,5,true>;
    using channel_6_t = gil::packed_dynamic_channel_reference<BitField,6,true>;

    using fixture_5_t = fixture::packed_dynamic_channel_reference<channel_5_t>;
    using fixture_6_t = fixture::packed_dynamic_channel_reference<channel_6_t>;

    std::uint16_t data_ = 0;
    channel_5_t channel1_;
    channel_6_t channel2_;
    channel_5_t channel3_;

    packed_dynamic_channels565()
        : channel1_(&data_, 0)
        , channel2_(&data_, 5)
        , channel3_(&data_, 11)
    {
        channel1_ = gil::channel_traits<channel_5_t>::max_value();
        channel2_ = gil::channel_traits<channel_6_t>::max_value();
        channel3_ = gil::channel_traits<channel_5_t>::max_value();
#ifdef BOOST_TEST_EQ
        BOOST_TEST_EQ(data_, 65535);
#endif
    }
};

}}}} // namespace boost::gil::test::fixture

#endif
