//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

// Example for resize_view() in the numeric extension

int main()
{
    namespace bg = boost::gil;

    bg::rgb8_image_t img;
    bg::read_image("test.jpg", img, bg::jpeg_tag{});

    // test resize_view
    // Scale the image to 100x100 pixels using bilinear resampling
    bg::rgb8_image_t square100x100(100, 100);
    bg::resize_view(bg::const_view(img), bg::view(square100x100), bg::bilinear_sampler{});
    bg::write_view("out-resize.jpg", bg::const_view(square100x100), bg::jpeg_tag{});

    return 0;
}
