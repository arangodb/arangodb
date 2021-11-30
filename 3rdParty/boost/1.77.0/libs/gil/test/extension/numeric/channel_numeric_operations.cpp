//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/channel_numeric_operations.hpp>

#include <boost/core/lightweight_test.hpp>

#include <tuple>
#include <type_traits>

#include "test_utility_output_stream.hpp"
#include "core/channel/test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct test_plus_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_plus_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_t(127));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_plus_integer_same_types{});
    }
};

struct test_plus_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        {
            using channel1_t = channel_t;
            using channel2_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            gil::channel_plus_t<channel1_t, channel2_t, channel1_t> f;
            BOOST_TEST_EQ(f(0, 0), channel1_t(0));
            BOOST_TEST_EQ(f(100, 27), channel_t(127));
        }
        {
            using channel1_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            using channel2_t = channel_t;
            gil::channel_plus_t<channel1_t, channel2_t, channel2_t> f;
            BOOST_TEST_EQ(f(0, 0), channel2_t(0));
            BOOST_TEST_EQ(f(100, 27), channel_t(127));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_plus_integer_mixed_types{});
    }
};

struct test_plus_integer_signed_types_with_overflow
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        // Signed integer overflow is UB, so just check addition does not yield mathematically
        // expected value but is constrained by the range of representable values for given type.

        auto const max_value = gil::channel_traits<channel_t>::max_value();
        gil::channel_plus_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_NE(f(max_value, 1), std::int64_t(max_value) + 1);
        BOOST_TEST_NE(f(max_value, max_value), std::int64_t(max_value) + max_value);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_signed_types>(test_plus_integer_signed_types_with_overflow{});
    }
};

struct test_plus_integer_unsigned_types_with_wraparound
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        // The C Standard, 6.2.5, paragraph 9 [ISO/IEC 9899:2011], states:
        // A computation involving unsigned operands can never overflow, because a result that
        // cannot be represented by the resulting unsigned integer type is reduced modulo the number
        // that is one greater than the largest value that can be represented by the resulting type.

        auto const max_value = gil::channel_traits<channel_t>::max_value();
        auto const min_value = gil::channel_traits<channel_t>::min_value();
        gil::channel_plus_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(max_value, 1), min_value);
        BOOST_TEST_EQ(f(max_value, max_value), max_value - 1);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_unsigned_types>(test_plus_integer_unsigned_types_with_wraparound{});
    }
};

struct test_minus_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_minus_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_t(73));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_minus_integer_same_types{});
    }
};

struct test_minus_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        {
            using channel1_t = channel_t;
            using channel2_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            gil::channel_minus_t<channel1_t, channel2_t, channel1_t> f;
            BOOST_TEST_EQ(f(0, 0), channel1_t(0));
            BOOST_TEST_EQ(f(100, 27), channel_t(73));
        }
        {
            using channel1_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            using channel2_t = channel_t;
            gil::channel_minus_t<channel1_t, channel2_t, channel2_t> f;
            BOOST_TEST_EQ(f(0, 0), channel2_t(0));
            BOOST_TEST_EQ(f(100, 27), channel_t(73));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_minus_integer_mixed_types{});
    }
};

struct test_multiplies_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_multiplies_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(1, 1), channel_t(1));
        BOOST_TEST_EQ(f(4, 2), channel_t(8));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_multiplies_integer_same_types{});
    }
};

struct test_multiplies_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        {
            using channel1_t = channel_t;
            using channel2_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            gil::channel_multiplies_t<channel1_t, channel2_t, channel1_t> f;
            BOOST_TEST_EQ(f(0, 0), channel1_t(0));
            BOOST_TEST_EQ(f(4, 2), channel_t(8));
        }
        {
            using channel1_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            using channel2_t = channel_t;
            gil::channel_multiplies_t<channel1_t, channel2_t, channel2_t> f;
            BOOST_TEST_EQ(f(0, 0), channel2_t(0));
            BOOST_TEST_EQ(f(4, 2), channel_t(8));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_multiplies_integer_mixed_types{});
    }
};

struct test_divides_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_divides_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 1), channel_t(0));
        BOOST_TEST_EQ(f(1, 1), channel_t(1));
        BOOST_TEST_EQ(f(4, 2), channel_t(2));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_divides_integer_same_types{});
    }
};

struct test_divides_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        {
            using channel1_t = channel_t;
            using channel2_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            gil::channel_divides_t<channel1_t, channel2_t, channel1_t> f;
            BOOST_TEST_EQ(f(0, 1), channel1_t(0));
            BOOST_TEST_EQ(f(4, 2), channel_t(2));
        }
        {
            using channel1_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
            using channel2_t = channel_t;
            gil::channel_divides_t<channel1_t, channel2_t, channel2_t> f;
            BOOST_TEST_EQ(f(0, 1), channel2_t(0));
            BOOST_TEST_EQ(f(4, 2), channel_t(2));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_divides_integer_mixed_types{});
    }
};

struct test_plus_scalar_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_plus_scalar_t<channel_t, int, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_t(127));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_plus_scalar_integer_same_types{});
    }
};

struct test_plus_scalar_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        using channel_result_t = std::uint8_t;
        gil::channel_plus_scalar_t<channel_t, int, channel_result_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_result_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_result_t(127));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_plus_scalar_integer_mixed_types{});
    }
};

struct test_minus_scalar_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_minus_scalar_t<channel_t, int, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_t(73));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_minus_scalar_integer_same_types{});
    }
};

struct test_minus_scalar_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        using channel_result_t = std::uint8_t;
        gil::channel_minus_scalar_t<channel_t, int, std::uint8_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_result_t(0));
        BOOST_TEST_EQ(f(100, 27), channel_result_t(73));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_minus_scalar_integer_mixed_types{});
    }
};

struct test_multiplies_scalar_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_multiplies_scalar_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_t(0));
        BOOST_TEST_EQ(f(1, 1), channel_t(1));
        BOOST_TEST_EQ(f(4, 2), channel_t(8));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_multiplies_scalar_integer_same_types{});
    }
};

struct test_multiplies_scalar_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        using channel_result_t = std::uint8_t;
        gil::channel_multiplies_scalar_t<channel_t, int, channel_result_t> f;
        BOOST_TEST_EQ(f(0, 0), channel_result_t(0));
        BOOST_TEST_EQ(f(4, 2), channel_result_t(8));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_multiplies_scalar_integer_mixed_types{});
    }
};

struct test_divides_scalar_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_divides_scalar_t<channel_t, channel_t, channel_t> f;
        BOOST_TEST_EQ(f(0, 1), channel_t(0));
        BOOST_TEST_EQ(f(1, 1), channel_t(1));
        BOOST_TEST_EQ(f(4, 2), channel_t(2));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_divides_scalar_integer_same_types{});
    }
};

struct test_divides_scalar_integer_mixed_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        using channel_result_t = std::uint8_t; // duplicates only one of fixture::channel_integer_types
        gil::channel_divides_scalar_t<channel_t, int, channel_result_t> f;
        BOOST_TEST_EQ(f(0, 1), channel_t(0));
        BOOST_TEST_EQ(f(4, 2), channel_t(2));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_divides_scalar_integer_mixed_types{});
    }
};

struct test_halves_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_halves_t<channel_t> f;
        {
            channel_t c(0);
            f(c);
            BOOST_TEST_EQ(c, channel_t(0));
        }
        {
            channel_t c(2);
            f(c);
            BOOST_TEST_EQ(c, channel_t(1));
        }
        {
            channel_t c(4);
            f(c);
            BOOST_TEST_EQ(c, channel_t(2));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_halves_integer_same_types{});
    }
};

struct test_zeros_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_zeros_t<channel_t> f;
        {
            channel_t c(0);
            f(c);
            BOOST_TEST_EQ(c, channel_t(0));
        }
        {
            channel_t c(2);
            f(c);
            BOOST_TEST_EQ(c, channel_t(0));
        }
        {
            channel_t c(4);
            f(c);
            BOOST_TEST_EQ(c, channel_t(0));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_zeros_integer_same_types{});
    }
};

struct test_assigns_integer_same_types
{
    template <typename Channel>
    void operator()(Channel const&)
    {
        using channel_t = Channel;
        gil::channel_assigns_t<channel_t, channel_t> f;
        {
            channel_t c1(10);
            channel_t c2(20);
            f(c1, c2);
            BOOST_TEST_EQ(c2, c1);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::channel_integer_types>(test_assigns_integer_same_types{});
    }
};

int main()
{
    test_plus_integer_same_types::run();
    test_plus_integer_mixed_types::run();
    test_plus_integer_signed_types_with_overflow::run();
    test_plus_integer_unsigned_types_with_wraparound::run();

    test_minus_integer_same_types::run();
    test_minus_integer_mixed_types::run();

    test_multiplies_integer_same_types::run();
    test_multiplies_integer_mixed_types::run();

    test_divides_integer_same_types::run();
    test_divides_integer_mixed_types::run();

    test_plus_scalar_integer_same_types::run();
    test_plus_scalar_integer_mixed_types::run();

    test_minus_scalar_integer_same_types::run();
    test_minus_scalar_integer_mixed_types::run();

    test_multiplies_scalar_integer_same_types::run();
    test_multiplies_scalar_integer_mixed_types::run();

    test_divides_scalar_integer_same_types::run();
    test_divides_scalar_integer_mixed_types::run();

    test_halves_integer_same_types::run();
    test_zeros_integer_same_types::run();
    test_assigns_integer_same_types::run();

    return ::boost::report_errors();
}
