//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/image_types/indexed_image.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_index_image()
{
    auto const pixel_generator = []() -> gil::rgb8_pixel_t {
        static int i = 0;
        i = (i > 255) ? 0 : (i + 1);
        auto const i8 = static_cast<std::uint8_t>(i);
        return gil::rgb8_pixel_t(i8, i8, i8);
    };

    {
        gil::indexed_image<std::uint8_t, gil::rgb8_pixel_t> img(640, 480);
        gil::fill_pixels(gil::view(img), gil::rgb8_pixel_t(255, 0, 0));

        gil::rgb8_pixel_t const p = *gil::view(img).xy_at(10, 10);
        BOOST_TEST_EQ(p[0], 255);
    }
    {
        using image_t = gil::indexed_image<gil::gray8_pixel_t, gil::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        gil::generate_pixels(img.get_indices_view(), []() -> gil::gray8_pixel_t {
            static int i = 0;
            i = (i > 255) ? 0 : (i + 1);
            auto const i8 = static_cast<std::uint8_t>(i);
            return gil::gray8_pixel_t(i8);
        });
        gil::generate_pixels(img.get_palette_view(), pixel_generator);

        gil::gray8_pixel_t index{0};
        index = *img.get_indices_view().xy_at(0, 0); // verify values along first row
        BOOST_TEST_EQ(static_cast<int>(index), (0 + 1));
        index = *img.get_indices_view().xy_at(128, 0);
        BOOST_TEST_EQ(static_cast<int>(index), (128 + 1));
        // verify wrapping of value by the pixels generator above
        index = *img.get_indices_view().xy_at(255, 0);
        BOOST_TEST_EQ(static_cast<int>(index), 0);

        // access via member function
        gil::rgb8_pixel_t const pixel1 = *img.get_palette_view().xy_at(index, 0);
        BOOST_TEST_EQ(pixel1[0], pixel1[1]);
        BOOST_TEST_EQ(pixel1[1], pixel1[2]);

        // access via free function
        gil::rgb8_pixel_t const pixel2 = *gil::view(img).xy_at(10, 1);
        BOOST_TEST_EQ(pixel2[0], pixel2[1]);
        BOOST_TEST_EQ(pixel2[1], pixel2[2]);
    }
    {
        using image_t = gil::indexed_image<gil::gray8_pixel_t, gil::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        gil::generate_pixels(img.get_indices_view(), []() -> uint8_t
            {
                static int i = 0;
                i = (i > 255) ? 0 : (i + 1);
                return static_cast<std::uint8_t>(i);
            });
        gil::generate_pixels(img.get_palette_view(), pixel_generator);

        std::uint8_t index = *img.get_indices_view().xy_at(128, 0);
        BOOST_TEST_EQ(static_cast<int>(index), (128 + 1));

        gil::rgb8_pixel_t const pixel1 = *img.get_palette_view().xy_at(index, 0);
        BOOST_TEST_EQ(pixel1[0], pixel1[1]);
        BOOST_TEST_EQ(pixel1[1], pixel1[2]);

        gil::rgb8_pixel_t const pixel2 = *view(img).xy_at(10, 1);
        BOOST_TEST_EQ(pixel2[0], pixel2[1]);
        BOOST_TEST_EQ(pixel2[1], pixel2[2]);
    }
    {
        using image_t = gil::indexed_image<std::uint8_t, gil::rgb8_pixel_t>;
        image_t img(640, 480, 256);

        for (image_t::y_coord_t y = 0; y < gil::view(img).height(); ++y)
        {
            image_t::view_t::x_iterator it = gil::view(img).row_begin(y);
            for (image_t::x_coord_t x = 0; x < gil::view(img).width(); ++x)
            {
                gil::rgb8_pixel_t p = *it;
                boost::ignore_unused(p);
                it++;
            }
        }

        // TODO: No checks? ~mloskot
    }
}

void test_index_image_view()
{
    // generate some data
    std::size_t const width = 640;
    std::size_t const height = 480;
    std::size_t const num_colors = 3;
    std::uint8_t const index = 2;

    // indices
    std::vector<std::uint8_t> indices(width * height, index);

    // colors
    std::vector<gil::rgb8_pixel_t> palette(num_colors);
    palette[0] = gil::rgb8_pixel_t(10, 20, 30);
    palette[1] = gil::rgb8_pixel_t(40, 50, 60);
    palette[2] = gil::rgb8_pixel_t(70, 80, 90);

    // create image views from raw memory
    auto indices_view = gil::interleaved_view(width, height,
        (gil::gray8_image_t::view_t::x_iterator) indices.data(),
        width); // row size in bytes

    auto palette_view = gil::interleaved_view(100, 1,
        (gil::rgb8_image_t::view_t::x_iterator) palette.data(),
        num_colors * 3); // row size in bytes

    auto ii_view = gil::view(indices_view, palette_view);

    auto p = ii_view(gil::point_t(0, 0));
    auto q = *ii_view.at(gil::point_t(0, 0));

    BOOST_TEST_EQ(gil::get_color(p, gil::red_t()), 70);
    BOOST_TEST_EQ(gil::get_color(p, gil::green_t()), 80);
    BOOST_TEST_EQ(gil::get_color(p, gil::blue_t()), 90);

    BOOST_TEST_EQ(gil::get_color(q, gil::red_t()), 70);
    BOOST_TEST_EQ(gil::get_color(q, gil::green_t()), 80);
    BOOST_TEST_EQ(gil::get_color(q, gil::blue_t()), 90);
}

int main()
{
    test_index_image();
    test_index_image_view();

    return ::boost::report_errors();
}
