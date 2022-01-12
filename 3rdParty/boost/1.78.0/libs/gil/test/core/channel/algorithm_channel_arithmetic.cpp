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

#include <type_traits>
#include <utility>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

template <typename ChannelFixtureBase>
void test_channel_arithmetic_mutable(std::false_type)  {}

template <typename ChannelFixtureBase>
void test_channel_arithmetic_mutable(std::true_type)
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
    BOOST_TEST_EQ(v, f.min_v_);

    f.min_v_ += one;
    f.min_v_ -= one;
    BOOST_TEST_EQ(v, f.min_v_);

    f.min_v_ *= one;
    f.min_v_ /= one;
    BOOST_TEST_EQ(v, f.min_v_);

    f.min_v_ = one; // assignable to scalar
    BOOST_TEST_EQ(f.min_v_, one);
    f.min_v_ = v; // and to value type
    BOOST_TEST_EQ(f.min_v_, v);

    // test swap
    channel_value_t v1 = f.min_v_;
    channel_value_t v2 = f.max_v_;
    std::swap(f.min_v_, f.max_v_);
    BOOST_TEST_GT(f.min_v_, f.max_v_);
    channel_value_t v3 = f.min_v_;
    channel_value_t v4 = f.max_v_;
    BOOST_TEST_EQ(v1, v4);
    BOOST_TEST_EQ(v2, v3);
}

template <typename ChannelFixtureBase>
void test_channel_arithmetic()
{
    using fixture_t = fixture::channel<ChannelFixtureBase>;
    fixture_t f;
    BOOST_TEST_EQ(f.min_v_ * 1, f.min_v_);
    BOOST_TEST_EQ(f.min_v_ / 1, f.min_v_);
    BOOST_TEST_EQ((f.min_v_ + 1) + 1, f.min_v_ + 2);
    BOOST_TEST_EQ((f.max_v_ - 1) - 1, f.max_v_ - 2);

    using is_mutable_t = std::integral_constant
        <
            bool,
            gil::channel_traits<typename fixture_t::channel_t>::is_mutable
        >;
    test_channel_arithmetic_mutable<ChannelFixtureBase>(is_mutable_t{});
}

struct test_channel_value
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        using fixture_t = fixture::channel_value<channel_t>;
        test_channel_arithmetic<fixture_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value{});
    }
};

struct test_channel_reference
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        using fixture_t = fixture::channel_reference<channel_t &>;
        test_channel_arithmetic<fixture_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_reference{});
    }
};

struct test_channel_reference_const
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        using fixture_t = fixture::channel_reference<channel_t const &>;
        test_channel_arithmetic<fixture_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_reference_const{});
    }
};

struct test_packed_channel_reference
{
    template <typename BitField>
    void operator()(BitField const &)
    {
        using bitfield_t = BitField;
        using channels565_t = fixture::packed_channels565<bitfield_t>;
        test_channel_arithmetic<typename channels565_t::fixture_0_5_t>();
        test_channel_arithmetic<typename channels565_t::fixture_5_6_t>();
        test_channel_arithmetic<typename channels565_t::fixture_11_5_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_bitfield_types>(test_packed_channel_reference{});
    }
};

struct test_packed_dynamic_channel_reference
{

    template <typename BitField>
    void operator()(BitField const &)
    {
        using bitfield_t = BitField;
        using channels565_t = fixture::packed_dynamic_channels565<bitfield_t>;
        test_channel_arithmetic<typename channels565_t::fixture_5_t>();
        test_channel_arithmetic<typename channels565_t::fixture_6_t>();
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_bitfield_types>(test_packed_dynamic_channel_reference{});
    }
};

int main()
{
    test_channel_value::run();
    test_channel_reference::run();
    test_channel_reference_const::run();
    test_packed_channel_reference::run();
    test_packed_dynamic_channel_reference::run();

    return ::boost::report_errors();
}
