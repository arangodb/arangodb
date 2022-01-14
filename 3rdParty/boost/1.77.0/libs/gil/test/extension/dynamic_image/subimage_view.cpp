//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/dynamic_image_all.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"
#include "core/image/test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct test_subimage_equals_image
{
    template <typename Image>
    void operator()(Image const&)
    {
        using image_t = Image;
        fixture::dynamic_image i0(fixture::create_image<image_t>(4, 4, 128));
        auto const v0 = gil::const_view(i0);
        BOOST_TEST_EQ(v0.dimensions().x, 4);
        BOOST_TEST_EQ(v0.dimensions().y, 4);
        BOOST_TEST_EQ(v0.size(), 4 * 4);

        // request with 2 x point_t values
        {
            auto v1 = gil::subimage_view(gil::view(i0), {0, 0}, i0.dimensions());
            BOOST_TEST_EQ(v0.dimensions(), v1.dimensions());
            BOOST_TEST_EQ(v0.size(), v1.size());
            BOOST_TEST(gil::equal_pixels(v0, v1));
        }
        // request with 4 x dimension values
        {
            auto v1 = gil::subimage_view(gil::view(i0), 0, 0, i0.dimensions().x, i0.dimensions().y);
            BOOST_TEST_EQ(v0.dimensions(), v1.dimensions());
            BOOST_TEST_EQ(v0.size(), v1.size());
            BOOST_TEST(gil::equal_pixels(v0, v1));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::image_types>(test_subimage_equals_image{});
    }
};

struct test_subimage_equals_image_quadrants
{
    template <typename Image>
    void operator()(Image const&)
    {
        using image_t = Image;
        fixture::dynamic_image i0(fixture::create_image<image_t>(4, 4, 0));
        auto v0 = gil::view(i0);
        // create test image and set values of pixels in:
        //  quadrant 1
        auto const i1 = fixture::create_image<image_t>(2, 2, 255);
        gil::apply_operation(v0, fixture::fill_any_view<image_t>({2, 3, 6, 7}, gil::const_view(i1)[0]));
        //  quadrant 2
        auto const i2 = fixture::create_image<image_t>(2, 2, 128);
        gil::apply_operation(v0, fixture::fill_any_view<image_t>({0, 1, 4, 5}, gil::const_view(i2)[0]));
        //  quadrant 3
        auto const i3 = fixture::create_image<image_t>(2, 2, 64);
        gil::apply_operation(v0, fixture::fill_any_view<image_t>({8, 9, 12, 13}, gil::const_view(i3)[0]));
        //  quadrant 4
        auto const i4 = fixture::create_image<image_t>(2, 2, 32);
        gil::apply_operation(v0, fixture::fill_any_view<image_t>({10, 11, 14, 15}, gil::const_view(i4)[0]));

        auto v1 = gil::subimage_view(gil::view(i0), { 2, 0 }, i0.dimensions() / 2);
        BOOST_TEST(gil::equal_pixels(v1, gil::const_view(i1)));
        auto v2 = gil::subimage_view(gil::view(i0), { 0, 0 }, i0.dimensions() / 2);
        BOOST_TEST(gil::equal_pixels(v2, gil::const_view(i2)));
        auto v3 = gil::subimage_view(gil::view(i0), { 0, 2 }, i0.dimensions() / 2);
        BOOST_TEST(gil::equal_pixels(v3, gil::const_view(i3)));
        auto v4 = gil::subimage_view(gil::view(i0), { 2, 2 }, i0.dimensions() / 2);
        BOOST_TEST(gil::equal_pixels(v4, gil::const_view(i4)));
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::image_types>(test_subimage_equals_image_quadrants{});
    }
};

int main()
{
    test_subimage_equals_image::run();
    test_subimage_equals_image_quadrants::run();

    return ::boost::report_errors();
}
