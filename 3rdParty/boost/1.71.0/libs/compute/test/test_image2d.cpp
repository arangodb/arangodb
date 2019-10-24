//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestImage2D
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/image/image2d.hpp>
#include <boost/compute/utility/dim.hpp>

#include "quirks.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(image2d_get_supported_formats)
{
    const std::vector<compute::image_format> formats =
        compute::image2d::get_supported_formats(context);
}

BOOST_AUTO_TEST_CASE(create_image_doctest)
{
    try {
//! [create_image]
// create 8-bit RGBA image format
boost::compute::image_format rgba8(CL_RGBA, CL_UNSIGNED_INT8);

// create 640x480 image object
boost::compute::image2d img(context, 640, 480, rgba8);
//! [create_image]

        // verify image has been created and format is correct
        BOOST_CHECK(img.get() != cl_mem());
        BOOST_CHECK(img.format() == rgba8);
        BOOST_CHECK_EQUAL(img.width(), size_t(640));
        BOOST_CHECK_EQUAL(img.height(), size_t(480));
    }
    catch(compute::opencl_error &e){
        if(e.error_code() == CL_IMAGE_FORMAT_NOT_SUPPORTED){
            // image format not supported by device
            return;
        }

        // some other error, rethrow
        throw;
    }
}

BOOST_AUTO_TEST_CASE(get_info)
{
    compute::image_format format(CL_RGBA, CL_UNSIGNED_INT8);

    if(!compute::image2d::is_supported_format(format, context)){
        std::cerr << "skipping get_info test, image format not supported" << std::endl;
        return;
    }

    compute::image2d image(
        context, 48, 64, format, compute::image2d::read_only
    );

    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_WIDTH), size_t(48));
    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_HEIGHT), size_t(64));
    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_DEPTH), size_t(0));
    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_ROW_PITCH), size_t(48*4));
    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_SLICE_PITCH), size_t(0));
    BOOST_CHECK_EQUAL(image.get_info<size_t>(CL_IMAGE_ELEMENT_SIZE), size_t(4));

    BOOST_CHECK(image.format() == format);
    BOOST_CHECK_EQUAL(image.width(), size_t(48));
    BOOST_CHECK_EQUAL(image.height(), size_t(64));
}

BOOST_AUTO_TEST_CASE(clone_image)
{
    compute::image_format format(CL_RGBA, CL_UNORM_INT8);

    if(!compute::image2d::is_supported_format(format, context)){
        std::cerr << "skipping clone_image test, image format not supported" << std::endl;
        return;
    }

    // image data
    unsigned int data[] = { 0x0000ffff, 0xff00ffff,
                            0x00ff00ff, 0xffffffff };

    // create image on the device
    compute::image2d image(context, 2, 2, format);

    // ensure we have a valid image object
    BOOST_REQUIRE(image.get() != cl_mem());

    // copy image data to the device
    queue.enqueue_write_image(image, image.origin(), image.size(), data);

    // clone image
    compute::image2d copy = image.clone(queue);

    // ensure image format is the same
    BOOST_CHECK(copy.format() == image.format());

    // read cloned image data back to the host
    unsigned int cloned_data[4];
    queue.enqueue_read_image(copy, image.origin(), image.size(), cloned_data);

    // ensure original data and cloned data are the same
    BOOST_CHECK_EQUAL(cloned_data[0], data[0]);
    BOOST_CHECK_EQUAL(cloned_data[1], data[1]);
    BOOST_CHECK_EQUAL(cloned_data[2], data[2]);
    BOOST_CHECK_EQUAL(cloned_data[3], data[3]);
}

#ifdef BOOST_COMPUTE_CL_VERSION_1_2
BOOST_AUTO_TEST_CASE(fill_image)
{
    REQUIRES_OPENCL_VERSION(1, 2); // device OpenCL version check

    compute::image_format format(CL_RGBA, CL_UNSIGNED_INT8);

    if(!compute::image2d::is_supported_format(format, context)){
        std::cerr << "skipping fill_image test, image format not supported" << std::endl;
        return;
    }

    compute::image2d img(context, 640, 480, format);

    // fill image with black
    compute::uint4_ black(0, 0, 0, 255);
    queue.enqueue_fill_image(img, &black, img.origin(), img.size());

    // read value of first pixel
    compute::uchar4_ first_pixel;
    queue.enqueue_read_image(img, compute::dim(0), compute::dim(1), &first_pixel);
    BOOST_CHECK_EQUAL(first_pixel, compute::uchar4_(0, 0, 0, 255));

    // fill image with white
    compute::uint4_ white(255, 255, 255, 255);
    queue.enqueue_fill_image(img, &white, img.origin(), img.size());

    // read value of first pixel
    queue.enqueue_read_image(img, compute::dim(0), compute::dim(1), &first_pixel);
    BOOST_CHECK_EQUAL(first_pixel, compute::uchar4_(255, 255, 255, 255));
}
#endif

// check type_name() for image2d
BOOST_AUTO_TEST_CASE(image2d_type_name)
{
    BOOST_CHECK(
        std::strcmp(
            boost::compute::type_name<boost::compute::image2d>(), "image2d_t"
        ) == 0
    );
}

BOOST_AUTO_TEST_CASE(map_image)
{
    compute::image_format format(CL_RGBA, CL_UNSIGNED_INT8);

    if(!compute::image2d::is_supported_format(format, context)){
        std::cerr << "skipping clone_image test, image format not supported" << std::endl;
        return;
    }

    // create image on the device
    compute::image2d image(context, 2, 2, format);

    // ensure we have a valid image object
    BOOST_REQUIRE(image.get() != cl_mem());

    size_t row_pitch = 0;
    size_t slice_pitch = 0;

    // write map image
    compute::uint_* ptr = static_cast<compute::uint_*>(
        queue.enqueue_map_image(image, CL_MAP_WRITE, image.origin(),
                                image.size(), row_pitch, slice_pitch)
    );

    BOOST_CHECK_EQUAL(row_pitch, size_t(2*4));

    // image data
    compute::uint_ data[] = { 0x0000ffff, 0xff00ffff,
                              0x00ff00ff, 0xffffffff };

    // copy data to image
    for(size_t i = 0; i < 4; i++){
        *(ptr+i) = data[i];
    }

    // unmap
    queue.enqueue_unmap_image(image, static_cast<void*>(ptr));

    // read map image
    compute::event map_event;
    ptr = static_cast<compute::uint_*>(
        queue.enqueue_map_image_async(image, CL_MAP_READ, image.origin(),
                                      image.size(), row_pitch, slice_pitch,
                                      map_event)
    );

    map_event.wait();

    BOOST_CHECK(map_event.get_status() == CL_COMPLETE);
    BOOST_CHECK_EQUAL(row_pitch, size_t(2*4));

    // checking
    for(size_t i = 0; i < 4; i++){
        BOOST_CHECK_EQUAL(*(ptr+i), data[i]);
    }

    // unmap
    queue.enqueue_unmap_image(image, static_cast<void*>(ptr));
}

BOOST_AUTO_TEST_SUITE_END()
