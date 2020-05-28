//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestImage1D
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/image/image1d.hpp>
#include <boost/compute/utility/dim.hpp>

#include "quirks.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(image1d_get_supported_formats)
{
    const std::vector<compute::image_format> formats =
        compute::image1d::get_supported_formats(context);
}

#ifdef BOOST_COMPUTE_CL_VERSION_1_2
BOOST_AUTO_TEST_CASE(fill_image1d)
{
    REQUIRES_OPENCL_VERSION(1, 2); // device OpenCL version check

    // single-channel unsigned char
    compute::image_format format(CL_R, CL_UNSIGNED_INT8);

    if(!compute::image1d::is_supported_format(format, context)){
        std::cerr << "skipping fill_image1d test, image format not supported" << std::endl;
        return;
    }

    compute::image1d img(context, 64, format);

    BOOST_CHECK_EQUAL(img.width(), size_t(64));
    BOOST_CHECK(img.size() == compute::dim(64));
    BOOST_CHECK(img.format() == format);

    // fill image with '128'
    compute::uint4_ fill_color(128, 0, 0, 0);
    queue.enqueue_fill_image(img, &fill_color, img.origin(), img.size());

    // read value of first pixel
    compute::uchar_ first_pixel = 0;
    queue.enqueue_read_image(img, compute::dim(0), compute::dim(1), &first_pixel);
    BOOST_CHECK_EQUAL(first_pixel, compute::uchar_(128));
}
#endif // BOOST_COMPUTE_CL_VERSION_1_2

// check type_name() for image1d
BOOST_AUTO_TEST_CASE(image1d_type_name)
{
    BOOST_CHECK(
        std::strcmp(
            boost::compute::type_name<boost::compute::image1d>(), "image1d_t"
        ) == 0
    );
}

BOOST_AUTO_TEST_SUITE_END()
