//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel_algorithm.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>

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
        BOOST_TEST_EQ(min_v, expect.min_v_);
        BOOST_TEST_EQ(max_v, expect.max_v_);
    }
};

//--- Test gil::channel_convert from integral channels to all byte channels -------------
template <typename SourceChannel, typename TargetChannel>
void test_channel_value_convert_from_integral()
{
    fixture::channel_minmax_value<SourceChannel> f;

    using channel_t = TargetChannel;
    // byte channel
    test_convert_to<fixture::channel_value<channel_t>>::from(f.min_v_, f.max_v_);
    test_convert_to<fixture::channel_reference<channel_t&>>::from(f.min_v_, f.max_v_);
    test_convert_to<fixture::channel_reference<channel_t const&>>::from(f.min_v_, f.max_v_);

    // packed_channel_reference
    {
        using channels565_t = fixture::packed_channels565<std::uint16_t>;
        test_convert_to<typename channels565_t::fixture_0_5_t>::from(f.min_v_, f.max_v_);
        test_convert_to<typename channels565_t::fixture_5_6_t>::from(f.min_v_, f.max_v_);
        test_convert_to<typename channels565_t::fixture_11_5_t>::from(f.min_v_, f.max_v_);
    }
    // packed_dynamic_channel_reference
    {
        using channels565_t = fixture::packed_dynamic_channels565<std::uint16_t>;
        test_convert_to<typename channels565_t::fixture_5_t>::from(f.min_v_, f.max_v_);
        test_convert_to<typename channels565_t::fixture_6_t>::from(f.min_v_, f.max_v_);
    }
}

struct test_channel_value_convert_from_uint8_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::uint8_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_uint8_t{});
    }
};

struct test_channel_value_convert_from_int8_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::int8_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_int8_t{});
    }
};

struct test_channel_value_convert_from_uint16_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::uint16_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_uint16_t{});
    }
};

struct test_channel_value_convert_from_int16_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::int16_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_int16_t{});
    }
};

struct test_channel_value_convert_from_uint32_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::uint32_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_uint32_t{});
    }
};

struct test_channel_value_convert_from_int32_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        test_channel_value_convert_from_integral<std::int32_t, channel_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value_convert_from_int32_t{});
    }
};

// FIXME: gil::float32_t <-> gil::float64_t seems not supported

//--- Test gil::channel_convert from gil::float32_t to all integer channels -------------
struct test_channel_value_convert_from_float32_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float32_t> f;
        test_convert_to<fixture::channel_value<channel_t>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_value_convert_from_float32_t{});
    }
};

struct test_channel_reference_convert_from_float32_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float32_t> f;
        test_convert_to<fixture::channel_reference<channel_t&>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_reference_convert_from_float32_t{});
    }
};

struct test_channel_reference_const_from_float32_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float32_t> f;
        test_convert_to<fixture::channel_reference<channel_t const &>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_reference_const_from_float32_t{});
    }
};

//--- Test gil::channel_convert from gil::float64_t to all integer channels -------------

struct test_channel_value_convert_from_float64_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float64_t> f;
        test_convert_to<fixture::channel_value<channel_t>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_value_convert_from_float64_t{});
    }
};

struct test_channel_reference_convert_from_float64_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float64_t> f;
        test_convert_to<fixture::channel_reference<channel_t &>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_reference_convert_from_float64_t{});
    }
};

struct test_channel_reference_const_convert_from_float64_t
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<gil::float64_t> f;
        test_convert_to<fixture::channel_reference<channel_t const &>>::from(f.min_v_, f.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_channel_reference_const_convert_from_float64_t{});
    }
};

int main()
{
    test_channel_value_convert_from_uint8_t::run();
    test_channel_value_convert_from_int8_t::run();
    test_channel_value_convert_from_uint16_t::run();
    test_channel_value_convert_from_int16_t::run();
    test_channel_value_convert_from_uint32_t::run();
    test_channel_value_convert_from_int32_t::run();

    test_channel_value_convert_from_float32_t::run();
    test_channel_reference_convert_from_float32_t::run();
    test_channel_reference_const_from_float32_t::run();

    test_channel_value_convert_from_float64_t::run();
    test_channel_reference_convert_from_float64_t::run();
    test_channel_reference_const_convert_from_float64_t::run();

    return ::boost::report_errors();
}
