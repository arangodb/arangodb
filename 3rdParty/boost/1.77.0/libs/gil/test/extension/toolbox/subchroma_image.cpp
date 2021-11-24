//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/ycbcr.hpp>
#include <boost/gil/extension/toolbox/image_types/subchroma_image.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11.hpp>

#include <vector>

namespace gil = boost::gil;
namespace mp11 = boost::mp11;

void test_subchroma_image()
{
    {
        gil::ycbcr_601_8_pixel_t a(10, 20, 30);
        gil::rgb8_pixel_t b;
        gil::bgr8_pixel_t c;

        gil::color_convert(a, b);
        gil::color_convert(a, c);
        BOOST_TEST(gil::static_equal(b, c));

        gil::color_convert(b, a);
    }
    {
        gil::ycbcr_709_8_pixel_t a(10, 20, 30);
        gil::rgb8_pixel_t b;
        gil::bgr8_pixel_t c;

        gil::color_convert(a, b);
        gil::color_convert(a, c);
        BOOST_TEST(gil::static_equal(b, c));

        gil::color_convert(b, a);
    }

    {
        using pixel_t = gil::rgb8_pixel_t;
        using image_t = gil::subchroma_image<pixel_t>;
        image_t img(320, 240);
        gil::fill_pixels(view(img), pixel_t(10, 20, 30));

        // TODO: Add BOOST_TEST checkpoints
    }
    {
        using pixel_t = gil::rgb8_pixel_t;

        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 4, 4>> a(640, 480);
        static_assert(a.ss_X == 1 && a.ss_Y == 1, "");
        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 4, 0>> b(640, 480);
        static_assert(b.ss_X == 1 && b.ss_Y == 2, "");
        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 2, 2>> c(640, 480);
        static_assert(c.ss_X == 2 && c.ss_Y == 1, "");
        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 2, 0>> d(640, 480);
        static_assert(d.ss_X == 2 && d.ss_Y == 2, "");
        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 1, 1>> e(640, 480);
        static_assert(e.ss_X == 4 && e.ss_Y == 1, "");
        gil::subchroma_image<pixel_t, mp11::mp_list_c<int, 4, 1, 0>> f(640, 480);
        static_assert(f.ss_X == 4 && f.ss_Y == 2, "");

        gil::fill_pixels(view(a), pixel_t(10, 20, 30));
        gil::fill_pixels(view(b), pixel_t(10, 20, 30));
        gil::fill_pixels(view(c), pixel_t(10, 20, 30));
        gil::fill_pixels(view(d), pixel_t(10, 20, 30));
        gil::fill_pixels(view(e), pixel_t(10, 20, 30));
        gil::fill_pixels(view(f), pixel_t(10, 20, 30));

        // TODO: Add BOOST_TEST checkpoints
    }
    {
        using pixel_t = gil::ycbcr_601_8_pixel_t;
        using factors_t = mp11::mp_list_c<int, 4, 2, 2>;
        using image_t = gil::subchroma_image<pixel_t, factors_t>;

        std::size_t const y_width = 320;
        std::size_t const y_height = 240;

        std::size_t image_size = (y_width * y_height)
            + (y_width / image_t::ss_X) * (y_height / image_t::ss_Y)
            + (y_width / image_t::ss_X) * (y_height / image_t::ss_Y);

        std::vector<unsigned char> data(image_size); // TODO: Initialize? --mloskot

        image_t::view_t v = gil::subchroma_view<pixel_t, factors_t>(y_width, y_height, &data.front());
        //gil::rgb8_pixel_t p;  // TODO: Why RGB?? --mloskot
        //p = *v.xy_at(0, 0);
        auto p = *v.xy_at(0, 0);

        // TODO: Add BOOST_TEST checkpoints
    }
}

int main()
{
    test_subchroma_image();

    return ::boost::report_errors();
}
