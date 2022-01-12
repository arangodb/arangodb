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
void test_channel_relation()
{
    using fixture_t = fixture::channel<ChannelFixtureBase>;
    using channel_value_t = typename fixture_t::channel_value_t;
    channel_value_t const one = 1;

    fixture_t f;
    BOOST_TEST_LE(f.min_v_, f.max_v_);
    BOOST_TEST_GE(f.max_v_, f.min_v_);
    BOOST_TEST_LT(f.min_v_, f.max_v_);
    BOOST_TEST_GT(f.max_v_, f.min_v_);
    BOOST_TEST_NE(f.max_v_, f.min_v_);
    BOOST_TEST_EQ(f.min_v_, f.min_v_);
#if !defined(BOOST_CLANG) || (__clang_major__ == 3 && __clang_minor__ >= 8)
    // This particular test fails with optimised build using clang 3.5 or 3.6
    // for unknown reasons. Volunteers are welcome to debug and confirm it is
    // either the compiler bug or the library bug:
    // b2 toolset=clang variant=release cxxstd=11 define=_GLIBCXX_USE_CXX11_ABI=0 libs/gil/test/core/channel//algorithm_channel_relation
    BOOST_TEST_NE(f.min_v_, one); // comparable to integral
#endif
}

struct test_channel_value
{
    template <typename Channel>
    void operator()(Channel const &)
    {
        using channel_t = Channel;
        using fixture_t = fixture::channel_value<channel_t>;
        test_channel_relation<fixture_t>();
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
        using fixture_t = fixture::channel_reference<channel_t&>;
        test_channel_relation<fixture_t>();
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
        using fixture_t = fixture::channel_reference<channel_t const&>;
        test_channel_relation<fixture_t>();
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
        test_channel_relation<typename channels565_t::fixture_0_5_t>();
        test_channel_relation<typename channels565_t::fixture_5_6_t>();
        test_channel_relation<typename channels565_t::fixture_11_5_t>();
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
        test_channel_relation<typename channels565_t::fixture_5_t>();
        test_channel_relation<typename channels565_t::fixture_6_t>();
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

    // TODO: packed_channel_reference_const ?
    // TODO: packed_dynamic_channel_reference_const ?

    return ::boost::report_errors();
}
