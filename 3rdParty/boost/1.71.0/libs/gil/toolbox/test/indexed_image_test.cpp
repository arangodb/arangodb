//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/image_types/indexed_image.hpp>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>

namespace bg = boost::gil;

BOOST_AUTO_TEST_SUITE(toolbox_tests)

BOOST_AUTO_TEST_CASE(index_image_test)
{
    auto const pixel_generator = []() -> bg::rgb8_pixel_t {
        static int i = 0;
        i = (i > 255) ? 0 : (i + 1);
        auto const i8 = static_cast<std::uint8_t>(i);
        return bg::rgb8_pixel_t(i8, i8, i8);
    };

    {
        bg::indexed_image<std::uint8_t, bg::rgb8_pixel_t> img(640, 480);
        bg::fill_pixels(bg::view(img), bg::rgb8_pixel_t(255, 0, 0));

        bg::rgb8_pixel_t const p = *bg::view(img).xy_at(10, 10);
        BOOST_TEST(p[0] == 255);
    }
    {
        using image_t = bg::indexed_image<bg::gray8_pixel_t, bg::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        generate_pixels(img.get_indices_view(), []() -> bg::gray8_pixel_t
        {
            static int i = 0;
            i = (i > 255) ? 0 : (i + 1);
            auto const i8 = static_cast<std::uint8_t>(i);
            return bg::gray8_pixel_t(i8);
        });
        generate_pixels(img.get_palette_view(), pixel_generator);

        bg::gray8_pixel_t index{0};
        index = *img.get_indices_view().xy_at(0, 0); // verify values along first row
        BOOST_TEST(static_cast<int>(index) == (0 + 1));
        index = *img.get_indices_view().xy_at(128, 0);
        BOOST_TEST(static_cast<int>(index) == (128 + 1));
        // verify wrapping of value by the pixels generator above
        index = *img.get_indices_view().xy_at(255, 0);
        BOOST_TEST(static_cast<int>(index) == 0);

        // access via member function
        bg::rgb8_pixel_t const pixel1 = *img.get_palette_view().xy_at(index, 0);
        BOOST_TEST(pixel1[0] == pixel1[1]);
        BOOST_TEST(pixel1[1] == pixel1[2]);

        // access via free function
        bg::rgb8_pixel_t const pixel2 = *bg::view(img).xy_at(10, 1);
        BOOST_TEST(pixel2[0] == pixel2[1]);
        BOOST_TEST(pixel2[1] == pixel2[2]);
    }
    {
        using image_t = bg::indexed_image<bg::gray8_pixel_t, bg::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        generate_pixels(img.get_indices_view(), []() -> uint8_t
        {
            static int i = 0;
            i = (i > 255) ? 0 : (i + 1);
            return static_cast<std::uint8_t>(i);
        });
        generate_pixels(img.get_palette_view(), pixel_generator);

        std::uint8_t index = *img.get_indices_view().xy_at(128, 0);
        BOOST_TEST(static_cast<int>(index) == (128 + 1));

        bg::rgb8_pixel_t const pixel1 = *img.get_palette_view().xy_at(index, 0);
        BOOST_TEST(pixel1[0] == pixel1[1]);
        BOOST_TEST(pixel1[1] == pixel1[2]);

        bg::rgb8_pixel_t const pixel2 = *view(img).xy_at(10, 1);
        BOOST_TEST(pixel2[0] == pixel2[1]);
        BOOST_TEST(pixel2[1] == pixel2[2]);
    }
    {
        using image_t = bg::indexed_image<std::uint8_t, bg::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        for (image_t::y_coord_t y = 0; y < bg::view(img).height(); ++y)
        {
            image_t::view_t::x_iterator it = bg::view(img).row_begin(y);
            for (image_t::x_coord_t x = 0; x < bg::view(img).width(); ++x)
            {
                bg::rgb8_pixel_t p = *it;
                boost::ignore_unused(p);
                it++;
            }
        }

        // TODO: No checks? ~mloskot
    }
}

BOOST_AUTO_TEST_CASE(index_image_view_test)
{
    // generate some data
    std::size_t const width = 640;
    std::size_t const height = 480;
    std::size_t const num_colors = 3;
    std::uint8_t const index = 2;

    // indices
    std::vector<std::uint8_t> indices(width * height, index);

    // colors
    std::vector<bg::rgb8_pixel_t> palette(num_colors);
    palette[0] = bg::rgb8_pixel_t(10, 20, 30);
    palette[1] = bg::rgb8_pixel_t(40, 50, 60);
    palette[2] = bg::rgb8_pixel_t(70, 80, 90);

    // create image views from raw memory
    auto indices_view = bg::interleaved_view(width, height,
        (bg::gray8_image_t::view_t::x_iterator) indices.data(),
        width); // row size in bytes

    auto palette_view = bg::interleaved_view(100, 1,
        (bg::rgb8_image_t::view_t::x_iterator) palette.data(),
        num_colors * 3); // row size in bytes

    auto ii_view = bg::view(indices_view, palette_view);

    auto p = ii_view(bg::point_t(0, 0));
    auto q = *ii_view.at(bg::point_t(0, 0));

    BOOST_ASSERT(bg::get_color(p, bg::red_t()) == 70);
    BOOST_ASSERT(bg::get_color(p, bg::green_t()) == 80);
    BOOST_ASSERT(bg::get_color(p, bg::blue_t()) == 90);

    BOOST_ASSERT(bg::get_color(q, bg::red_t()) == 70);
    BOOST_ASSERT(bg::get_color(q, bg::green_t()) == 80);
    BOOST_ASSERT(bg::get_color(q, bg::blue_t()) == 90);
}

BOOST_AUTO_TEST_SUITE_END()
