//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Mageswaran.D <mageswaran1989@gmail.com>
//
// Book Refered: OpenCL Programming Guide
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
// About Sobel Filter:
// * Edge Filter - distinguishes the differrent color region
// * Finds the gradient in x and y-axes
// * Three step process
//   -> Find x-axis gradient with kernel/matrix
//           Gx = [-1 0 +1]
//                [-2 0 +2]
//                [-1 0 +1]
//   -> Find y-axis gradient with kernel/matrix
//           Gy = [-1 -2 -1]
//                [ 0  0  0]
//                [+1 +2 +1]
// * Gradient magnitude G = sqrt(Gx^2 + Gy^2)
//---------------------------------------------------------------------------//

#include <iostream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/interop/opencv/core.hpp>
#include <boost/compute/interop/opencv/highgui.hpp>
#include <boost/compute/utility/source.hpp>

#include <boost/program_options.hpp>

namespace compute = boost::compute;
namespace po = boost::program_options;

// Create sobel filter program
const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE (
    //For out of boundary pixels, edge pixel
    // value is returned
    const sampler_t sampler = CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    kernel void sobel_rgb(read_only image2d_t src, write_only image2d_t dst)
    {
        int x = (int)get_global_id(0);
        int y = (int)get_global_id(1);

        if (x >= get_image_width(src) || y >= get_image_height(src))
                return;

        //  [(x-1, y+1), (x, y+1), (x+1, y+1)]
        //  [(x-1, y  ), (x, y  ), (x+1, y  )]
        //  [(x-1, y-1), (x, y-1), (x+1, y-1)]

        //  [p02, p12,   p22]
        //  [p01, pixel, p21]
        //  [p00, p10,   p20]

        //Basically finding influence of neighbour pixels on current pixel
        float4 p00 = read_imagef(src, sampler, (int2)(x - 1, y - 1));
        float4 p10 = read_imagef(src, sampler, (int2)(x,     y - 1));
        float4 p20 = read_imagef(src, sampler, (int2)(x + 1, y - 1));

        float4 p01 = read_imagef(src, sampler, (int2)(x - 1, y));
        //pixel that we are working on
        float4 p21 = read_imagef(src, sampler, (int2)(x + 1, y));

        float4 p02 = read_imagef(src, sampler, (int2)(x - 1, y + 1));
        float4 p12 = read_imagef(src, sampler, (int2)(x,     y + 1));
        float4 p22 = read_imagef(src, sampler, (int2)(x + 1, y + 1));

        //Find Gx = kernel + 3x3 around current pixel
        //           Gx = [-1 0 +1]     [p02, p12,   p22]
        //                [-2 0 +2]  +  [p01, pixel, p21]
        //                [-1 0 +1]     [p00, p10,   p20]
        float3 gx = -p00.xyz + p20.xyz +
                    2.0f * (p21.xyz - p01.xyz)
                    -p02.xyz + p22.xyz;

        //Find Gy = kernel + 3x3 around current pixel
        //           Gy = [-1 -2 -1]     [p02, p12,   p22]
        //                [ 0  0  0]  +  [p01, pixel, p21]
        //                [+1 +2 +1]     [p00, p10,   p20]
        float3 gy = p00.xyz + p20.xyz +
                    2.0f * (- p12.xyz + p10.xyz) -
                    p02.xyz - p22.xyz;
        //Find G
        float3 g = native_sqrt(gx * gx + gy * gy);

        // we could also approximate this as g = fabs(gx) + fabs(gy)
        write_imagef(dst, (int2)(x, y), (float4)(g.x, g.y, g.z, 1.0f));
    }
);

// This example shows how to apply sobel filter on images or on camera frames
// with OpenCV, transfer the frames to the GPU, and apply a sobel filter
// written in OpenCL
int main(int argc, char *argv[])
{
    ///////////////////////////////////////////////////////////////////////////

    // setup the command line arguments
    po::options_description desc;
    desc.add_options()
            ("help",  "show available options")
            ("camera", po::value<int>()->default_value(-1),
                                 "if not default camera, specify a camera id")
            ("image", po::value<std::string>(), "path to image file");

    // Parse the command lines
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //check the command line arguments
    if(vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    ///////////////////////////////////////////////////////////////////////////

    //OpenCV variables
    cv::Mat cv_mat;
    cv::VideoCapture cap; //OpenCV camera handle.

    //OpenCL variables
    // Get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    compute::program filter_program =
            compute::program::create_with_source(source, context);

    try
    {
        filter_program.build();
    }
    catch(compute::opencl_error e)
    {
        std::cout<<"Build Error: "<<std::endl
                 <<filter_program.build_log();
    }

    // create fliter kernel and set arguments
    compute::kernel filter_kernel(filter_program, "sobel_rgb");

    ///////////////////////////////////////////////////////////////////////////

    //check for image paths
    if(vm.count("image"))
    {
        // Read image with OpenCV
        cv_mat = cv::imread(vm["image"].as<std::string>(),
                                       CV_LOAD_IMAGE_COLOR);
        if(!cv_mat.data){
            std::cerr << "Failed to load image" << std::endl;
            return -1;
        }
    }
    else //by default use camera
    {
        //open camera
        cap.open(vm["camera"].as<int>());
        // read first frame
        cap >> cv_mat;
        if(!cv_mat.data){
            std::cerr << "failed to capture frame" << std::endl;
            return -1;
        }
    }

    // Convert image to BGRA (OpenCL requires 16-byte aligned data)
    cv::cvtColor(cv_mat, cv_mat, CV_BGR2BGRA);

    // Transfer image/frame data to gpu
    compute::image2d dev_input_image =
            compute::opencv_create_image2d_with_mat(
                cv_mat, compute::image2d::read_write, queue
                );

    // Create output image
    // Be sure what will be your ouput image/frame size
    compute::image2d dev_output_image(
                context,
                dev_input_image.width(),
                dev_input_image.height(),
                dev_input_image.format(),
                compute::image2d::write_only
                );

    filter_kernel.set_arg(0, dev_input_image);
    filter_kernel.set_arg(1, dev_output_image);


    // run flip kernel
    size_t origin[2] = { 0, 0 };
    size_t region[2] = { dev_input_image.width(),
                         dev_input_image.height() };

    ///////////////////////////////////////////////////////////////////////////

    queue.enqueue_nd_range_kernel(filter_kernel, 2, origin, region, 0);

    //check for image paths
    if(vm.count("image"))
    {
        // show host image
        cv::imshow("Original Image", cv_mat);

        // show gpu image
        compute::opencv_imshow("Filtered Image", dev_output_image, queue);

        // wait and return
        cv::waitKey(0);
    }
    else
    {
        char key = '\0';
        while(key != 27) //check for escape key
        {
            cap >> cv_mat;

            // Convert image to BGRA (OpenCL requires 16-byte aligned data)
            cv::cvtColor(cv_mat, cv_mat, CV_BGR2BGRA);

            // Update the device image memory with current frame data
            compute::opencv_copy_mat_to_image(cv_mat,
                                              dev_input_image,queue);

            // Run the kernel on the device
            queue.enqueue_nd_range_kernel(filter_kernel, 2, origin, region, 0);

            // Show host image
            cv::imshow("Camera Frame", cv_mat);

            // Show GPU image
            compute::opencv_imshow("Filtered RGB Frame", dev_output_image, queue);

            // wait
            key = cv::waitKey(10);
        }
    }
    return 0;
}
