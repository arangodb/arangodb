//
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/core/lightweight_test.hpp>

#include "test_utility_output_stream.hpp"

#include <cstdint>
#include <sstream>
#include <type_traits>

namespace utility = boost::gil::test::utility;

int main()
{
    static_assert(
        std::is_same<utility::printable_numeric_t<std::uint8_t>, int>::value,
        "std::uint8_t not printable as int");
    static_assert(
        !std::is_same<utility::printable_numeric_t<std::uint64_t>, int>::value,
        "std::uint64_t printable as int");
    static_assert(
        std::is_same<utility::printable_numeric_t<float>, double>::value,
        "float not printable as double");

    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(std::int8_t{-128});
        BOOST_TEST_EQ(oss.str(), "v0=-128");
    }
    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(std::uint8_t{128});
        BOOST_TEST_EQ(oss.str(), "v0=128");
    }
    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(std::int16_t{-32768});
        BOOST_TEST_EQ(oss.str(), "v0=-32768");
    }
    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(std::uint16_t{32768});
        BOOST_TEST_EQ(oss.str(), "v0=32768");
    }
    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(float{1.2345f});
        BOOST_TEST_EQ(oss.str(), "v0=1.2345");
    }
    {
        std::ostringstream oss;
        utility::print_color_base p{oss};
        p(double{1.2345});
        BOOST_TEST_EQ(oss.str(), "v0=1.2345");
    }

    return ::boost::report_errors();
}
