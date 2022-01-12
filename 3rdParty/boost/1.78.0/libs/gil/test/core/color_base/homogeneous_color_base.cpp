//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/color_base.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/gil/rgba.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/core/typeinfo.hpp>

#include <type_traits>

namespace gil = boost::gil;

namespace {

// Unknown layout is used where layout mapping is irrelevant for a test and its result.
using unknown_layout_t = gil::gray_layout_t;

template <int N>
using color_base = gil::detail::homogeneous_color_base<std::uint8_t, unknown_layout_t, N>;

std::integral_constant<int, 0> e0;
std::integral_constant<int, 1> e1;
std::integral_constant<int, 2> e2;
std::integral_constant<int, 3> e3;
std::integral_constant<int, 4> e4;

} // unnamed namespace

void test_homogeneous_color_base_1_default_constructor()
{
    using fixture = color_base<1>;
    fixture f;
    BOOST_TEST_EQ(std::uint8_t{f}, std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{0});
}

void test_homogeneous_color_base_1_value_constructor()
{
    using fixture = color_base<1>;
    fixture f{1};
    BOOST_TEST_EQ(std::uint8_t{f}, std::uint8_t{1});
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{1});
}

void test_homogeneous_color_base_2_default_constructor()
{
    using fixture = color_base<2>;
    fixture f;
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e1), std::uint8_t{0});
}

void test_homogeneous_color_base_2_value_constructor()
{
    using fixture = color_base<2>;
    fixture f{2};
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{2});
    BOOST_TEST_EQ(f.at(e0), f.at(e1));
}

void test_homogeneous_color_base_3_default_constructor()
{
    using fixture = color_base<3>;
    fixture f;
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e1), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e2), std::uint8_t{0});
}

void test_homogeneous_color_base_3_value_constructor()
{
    using fixture = color_base<3>;
    fixture f{3};
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{3});
    BOOST_TEST_EQ(f.at(e0), f.at(e1));
    BOOST_TEST_EQ(f.at(e0), f.at(e2));
}

void test_homogeneous_color_base_4_default_constructor()
{
    using fixture = color_base<4>;
    fixture f;
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e1), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e2), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e3), std::uint8_t{0});
}

void test_homogeneous_color_base_4_value_constructor()
{
    using fixture = color_base<4>;
    fixture f{4};
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{4});
    BOOST_TEST_EQ(f.at(e0), f.at(e1));
    BOOST_TEST_EQ(f.at(e0), f.at(e2));
    BOOST_TEST_EQ(f.at(e0), f.at(e3));
}

void test_homogeneous_color_base_5_default_constructor()
{
    using fixture = color_base<5>;
    fixture f;
    BOOST_TEST_EQ(f.at(e0), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e1), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e2), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e3), std::uint8_t{0});
    BOOST_TEST_EQ(f.at(e4), std::uint8_t{0});
}

void test_homogeneous_color_base_5_value_constructor()
{
    using fixture = color_base<5>;
    fixture f{5};
    BOOST_TEST_EQ(f.at(e0), f.at(e1));
    BOOST_TEST_EQ(f.at(e0), f.at(e2));
    BOOST_TEST_EQ(f.at(e0), f.at(e3));
    BOOST_TEST_EQ(f.at(e0), f.at(e4));
}

int main()
{
    test_homogeneous_color_base_1_default_constructor();
    test_homogeneous_color_base_1_value_constructor();
    test_homogeneous_color_base_2_default_constructor();
    test_homogeneous_color_base_2_value_constructor();
    test_homogeneous_color_base_3_default_constructor();
    test_homogeneous_color_base_3_value_constructor();
    test_homogeneous_color_base_4_default_constructor();
    test_homogeneous_color_base_4_value_constructor();
    test_homogeneous_color_base_5_default_constructor();
    test_homogeneous_color_base_5_value_constructor();

    return ::boost::report_errors();
}
