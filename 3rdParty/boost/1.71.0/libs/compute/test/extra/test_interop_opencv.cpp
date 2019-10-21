//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInteropOpenCV
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/algorithm/reverse.hpp>
#include <boost/compute/interop/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bcl = boost::compute;

BOOST_AUTO_TEST_CASE(opencv_mat_to_buffer)
{
    // create opencv mat
    cv::Mat mat(1, 4, CV_32F);
    mat.at<float>(0, 0) = 0.0f;
    mat.at<float>(0, 1) = 2.5f;
    mat.at<float>(0, 2) = 4.1f;
    mat.at<float>(0, 3) = 5.6f;

    // copy mat to gpu vector
    bcl::vector<float> vector(4, context);
    bcl::opencv_copy_mat_to_buffer(mat, vector.begin(), queue);
    CHECK_RANGE_EQUAL(float, 4, vector, (0.0f, 2.5f, 4.1f, 5.6f));

    // reverse gpu vector and copy back to mat
    bcl::reverse(vector.begin(), vector.end(), queue);
    bcl::opencv_copy_buffer_to_mat(vector.begin(), mat, queue);
    BOOST_CHECK_EQUAL(mat.at<float>(0), 5.6f);
    BOOST_CHECK_EQUAL(mat.at<float>(1), 4.1f);
    BOOST_CHECK_EQUAL(mat.at<float>(2), 2.5f);
    BOOST_CHECK_EQUAL(mat.at<float>(3), 0.0f);
}

BOOST_AUTO_TEST_CASE(opencv_image_format)
{
    // 8-bit uchar BGRA
    BOOST_CHECK(
        bcl::opencv_get_mat_image_format(cv::Mat(32, 32, CV_8UC4)) ==
        bcl::image_format(CL_BGRA, CL_UNORM_INT8)
    );

    // 32-bit float
    BOOST_CHECK(
        bcl::opencv_get_mat_image_format(cv::Mat(32, 32, CV_32F)) ==
        bcl::image_format(CL_INTENSITY, CL_FLOAT)
    );

    // 32-bit float RGBA
    BOOST_CHECK(
        bcl::opencv_get_mat_image_format(cv::Mat(32, 32, CV_32FC4)) ==
        bcl::image_format(CL_RGBA, CL_FLOAT)
    );
   
    // 16-bit uchar BGRA 
    BOOST_CHECK(
        bcl::opencv_get_mat_image_format(cv::Mat(32, 32, CV_16UC4)) ==
        bcl::image_format(CL_BGRA, CL_UNORM_INT16)
    );

    // 8-bit uchar
    BOOST_CHECK(
        bcl::opencv_get_mat_image_format(cv::Mat(32, 32, CV_8UC1)) ==
        bcl::image_format(CL_INTENSITY, CL_UNORM_INT8)
    );
}

BOOST_AUTO_TEST_CASE(opencv_float_mat_image2d)
{
    REQUIRES_OPENCL_VERSION(1,2);

    cv::Vec4f pixel;
    // create opencv mat
    cv::Mat mat(2, 2, CV_32FC4, cv::Scalar(100, 150, 200, 255));
    // transfer image to gpu
    bcl::image2d image =
            bcl::opencv_create_image2d_with_mat(
                mat, 
                bcl::image2d::read_only,
                queue
                );
    // copy the data back to cpu
    bcl::opencv_copy_image_to_mat(image, mat, queue);
    
    pixel = mat.at<cv::Vec4f>(1,1);
    BOOST_CHECK_EQUAL(pixel[0], 100.0f);
    BOOST_CHECK_EQUAL(pixel[1], 150.0f);
    BOOST_CHECK_EQUAL(pixel[2], 200.0f);
    BOOST_CHECK_EQUAL(pixel[3], 255.0f);
}

BOOST_AUTO_TEST_SUITE_END()
