//
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

namespace boost { namespace gil { namespace test { namespace fixture {

gil::gray8_pixel_t gray8_back_pixel{128};
gil::gray8_pixel_t gray8_draw_pixel{64};
gil::rgb8_pixel_t rgb8_back_pixel{255, 128, 64};
gil::rgb8_pixel_t rgb8_draw_pixel{64, 32, 16};

auto make_image_gray8() -> gil::gray8_image_t
{
    gil::gray8_image_t image(2, 2, gray8_back_pixel);
    auto view = gil::view(image);
    view(0, 0) = gray8_draw_pixel;
    view(1, 1) = gray8_draw_pixel;
    return image;
}

auto make_image_rgb8() -> gil::rgb8_image_t
{
    gil::rgb8_image_t image(2, 2, rgb8_back_pixel);
    auto view = gil::view(image);
    view(0, 0) = rgb8_draw_pixel;
    view(1, 1) = rgb8_draw_pixel;
    return image;
}

}}}}
