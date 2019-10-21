//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/mpl/vector.hpp>

int main()
{
    namespace gil = boost::gil;

    using my_images_t = boost::mpl::vector<gil::gray8_image_t, gil::rgb8_image_t, gil::gray16_image_t, gil::rgb16_image_t>;
    gil::any_image<my_images_t> dynamic_image;
    gil::read_image("test.jpg", dynamic_image, gil::jpeg_tag());
    // Save the image upside down, preserving its native color space and channel depth
    auto view = gil::flipped_up_down_view(gil::const_view(dynamic_image));
    gil::write_view("out-dynamic_image.jpg", view, gil::jpeg_tag());
}
