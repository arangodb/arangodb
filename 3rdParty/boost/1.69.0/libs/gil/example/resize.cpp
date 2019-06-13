//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

// Example for resize_view() in the numeric extension

int main()
{
    using namespace boost::gil;

    rgb8_image_t img;
    jpeg_read_image("test.jpg",img);

    // test resize_view
    // Scale the image to 100x100 pixels using bilinear resampling
    rgb8_image_t square100x100(100,100);
    resize_view(const_view(img), view(square100x100), bilinear_sampler());
    jpeg_write_view("out-resize.jpg",const_view(square100x100));

    return 0;
}
