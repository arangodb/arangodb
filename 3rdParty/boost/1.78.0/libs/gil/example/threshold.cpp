//
// Copyright 2019 Miral Shah <miralshah2211@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/image_processing/threshold.hpp>

using namespace boost::gil;

int main()
{
    rgb8_image_t img;
    read_image("test.jpg",img, jpeg_tag{});
    rgb8_image_t img_out(img.dimensions());

//    performing binary threshold on each channel of the image
//    if the pixel value is more than 150 than it will be set to 255 else to 0
    boost::gil::threshold_binary(const_view(img), view(img_out), 150, 255);
    write_view("out-threshold-binary.jpg", view(img_out), jpeg_tag{});

//    if the pixel value is more than 150 than it will be set to 150 else no change
    boost::gil::threshold_truncate(const_view(img), view(img_out), 150, threshold_truncate_mode::threshold);
    write_view("out-threshold-binary_inv.jpg", view(img_out), jpeg_tag{});

    return 0;
}
