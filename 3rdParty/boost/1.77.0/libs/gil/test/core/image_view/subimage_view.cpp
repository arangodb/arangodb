//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

#include <boost/core/lightweight_test.hpp>

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
        auto i0 = fixture::create_image<image_t>(4, 4, 128);
        auto const v0 = gil::const_view(i0);
        BOOST_TEST_EQ(v0.dimensions().x, 4);
        BOOST_TEST_EQ(v0.dimensions().y, 4);

        // request with 2 x point_t values
        {
            auto v1 = gil::subimage_view(gil::view(i0), {0, 0}, i0.dimensions());
            BOOST_TEST_EQ(v0.dimensions(), v1.dimensions());
            BOOST_TEST(gil::equal_pixels(v0, v1));
        }
        // request with 4 x dimension values
        {
            auto v1 = gil::subimage_view(gil::view(i0), 0, 0, i0.dimensions().x, i0.dimensions().y);
            BOOST_TEST_EQ(v0.dimensions(), v1.dimensions());
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
        auto i0 = fixture::create_image<image_t>(4, 4, 0);
        auto v0 = gil::view(i0);
        // create test image and set values of pixels in:
        //  quadrant 1
        auto const i1 = fixture::create_image<image_t>(2, 2, 255);
        v0[2] = v0[3] = v0[6] = v0[7] = gil::const_view(i1)[0];
        //  quadrant 2
        auto const i2 = fixture::create_image<image_t>(2, 2, 128);
        v0[0] = v0[1] = v0[4] = v0[5] = gil::const_view(i2)[0];
        //  quadrant 3
        auto const i3 = fixture::create_image<image_t>(2, 2, 64);
        v0[8] = v0[9] = v0[12] = v0[13] = gil::const_view(i3)[0];
        //  quadrant 4
        auto const i4 = fixture::create_image<image_t>(2, 2, 32);
        v0[10] = v0[11] = v0[14] = v0[15] = gil::const_view(i4)[0];

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
