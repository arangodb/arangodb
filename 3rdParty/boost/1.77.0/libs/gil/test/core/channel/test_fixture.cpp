//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include "test_fixture.hpp"

#include <boost/core/lightweight_test.hpp>

#include <limits>

namespace fixture = boost::gil::test::fixture;

struct test_channel_minmax_value_float
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_minmax_value<channel_t> fix;
        fixture::channel_minmax_value<channel_t> exp;
        BOOST_TEST_EQ(fix.min_v_, exp.min_v_);
        BOOST_TEST_EQ(fix.max_v_, exp.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_bitfield_types>(test_channel_minmax_value_float{});
    }
};

struct test_channel_value
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        fixture::channel_value<channel_t> fix;
        fixture::channel_minmax_value<channel_t> exp;
        BOOST_TEST_EQ(fix.min_v_, exp.min_v_);
        BOOST_TEST_EQ(fix.max_v_, exp.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_value{});
    }
};

struct test_channel_reference
{
        template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        fixture::channel_reference<channel_t &> fix;
        fixture::channel_minmax_value<channel_t> exp;
        BOOST_TEST_EQ(fix.min_v_, exp.min_v_);
        BOOST_TEST_EQ(fix.max_v_, exp.max_v_);
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
        fixture::channel_reference<channel_t const &> fix;
        fixture::channel_minmax_value<channel_t> exp;
        BOOST_TEST_EQ(fix.min_v_, exp.min_v_);
        BOOST_TEST_EQ(fix.max_v_, exp.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_byte_types>(test_channel_reference_const{});
    }
};

struct test_packed_channels565
{
    template <typename BitField>
    void operator()(BitField const &)
    {
        using bitfield_t = BitField;
        static_assert(std::is_integral<bitfield_t>::value, "bitfield is not integral type");

        // Regardless of BitField buffer bit-size, the fixture is initialized
        // with max value that fits into 5+6+5 bit integer
        fixture::packed_channels565<bitfield_t> fix;
        fixture::channel_minmax_value<std::uint16_t> exp;
        BOOST_TEST_EQ(fix.data_, exp.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_bitfield_types>(test_packed_channels565{});
    }
};

struct test_packed_dynamic_channels565
{
    template <typename BitField>
    void operator()(BitField const &)
    {
        using bitfield_t = BitField;
        static_assert(std::is_integral<bitfield_t>::value, "bitfield is not integral type");

        // Regardless of BitField buffer bit-size, the fixture is initialized
        // with max value that fits into 5+6+5 bit integer
        fixture::packed_dynamic_channels565<bitfield_t> fix;
        fixture::channel_minmax_value<std::uint16_t> exp;
        BOOST_TEST_EQ(fix.data_, exp.max_v_);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_bitfield_types>(test_packed_dynamic_channels565{});
    }
};

int main()
{
    test_channel_minmax_value_float::run();
    test_channel_value::run();
    test_channel_reference::run();
    test_channel_reference_const::run();
    test_packed_channels565::run();
    test_packed_dynamic_channels565::run();

    return ::boost::report_errors();
}
