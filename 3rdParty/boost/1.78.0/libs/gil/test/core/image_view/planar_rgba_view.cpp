//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>

#include "test_utility_output_stream.hpp"

namespace boost { namespace gil {
namespace test { namespace fixture {

gil::point_t d{2, 2};
std::uint8_t r[] = { 1, 2, 3, 4 };
std::uint8_t g[] = { 10, 20, 30, 40 };
std::uint8_t b[] = { 110, 120, 130, 140 };
std::uint8_t a[] = { 251, 252, 253, 254 };

}}}}

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

void test_dimensions()
{
    auto v = gil::planar_rgba_view(fixture::d.x, fixture::d.y, fixture::r, fixture::g, fixture::b, fixture::a, sizeof(std::uint8_t) * 2);
    BOOST_TEST(!v.empty());
    BOOST_TEST_EQ(v.dimensions(), fixture::d);
    BOOST_TEST_EQ(v.num_channels(), 4u);
    BOOST_TEST_EQ(v.size(), static_cast<std::size_t>(fixture::d.x * fixture::d.y));
}

void test_front()
{
    auto v = gil::planar_rgba_view(fixture::d.x, fixture::d.y, fixture::r, fixture::g, fixture::b, fixture::a, sizeof(std::uint8_t) * 2);
    gil::rgba8_pixel_t const pf{1, 10, 110, 251};
    BOOST_TEST_EQ(v.front(), pf);
}

void test_back()
{
    auto v = gil::planar_rgba_view(fixture::d.x, fixture::d.y, fixture::r, fixture::g, fixture::b, fixture::a, sizeof(std::uint8_t) * 2);
    gil::rgba8_pixel_t const pb{4, 40, 140, 254};
    BOOST_TEST_EQ(v.back(), pb);
}

void test_pixel_equal_to_operator()
{
    auto v = gil::planar_rgba_view(fixture::d.x, fixture::d.y, fixture::r, fixture::g, fixture::b, fixture::a, sizeof(std::uint8_t) * 2);
    for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(v.size()); i++)
    {
        gil::rgba8_pixel_t const p{fixture::r[i], fixture::g[i], fixture::b[i], fixture::a[i]};
        BOOST_TEST_EQ(v[i], p);
    }
}

int main()
{
    test_dimensions();
    test_front();
    test_back();
    test_pixel_equal_to_operator();

    return ::boost::report_errors();
}
