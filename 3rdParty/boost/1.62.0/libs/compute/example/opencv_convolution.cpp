//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Mageswaran.D <mageswaran1989@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
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

// Create convolution program
const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE (
    __kernel void convolution(__read_only  image2d_t  sourceImage,
                              __write_only image2d_t  outputImage,
                              __constant float* filter,
                              int filterWidth)
    {
        const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                                  CLK_ADDRESS_CLAMP_TO_EDGE   |
                                  CLK_FILTER_NEAREST;

        // Store each work-item's unique row and column
        int x   = get_global_id(0);
        int y   = get_global_id(1);

        // Half the width of the filter is needed for indexing
        // memory later
        int halfWidth = (int)(filterWidth/2);

        // All accesses to images return data as four-element vector
        // (i.e., float4).
        float4 sum = {0.0f, 0.0f, 0.0f, 0.0f};

        // Iterator for the filter
        int filterIdx = 0;

        // Each work-item iterates around its local area based on the
        // size of the filter
        int2 coords;  // Coordinates for accessing the image

        // Iterate the filter rows
        for(int i = -halfWidth; i <= halfWidth; i++)
        {
            coords.y = y + i;

            // Iterate over the filter columns
            for(int j = -halfWidth; j <= halfWidth; j++)
            {
                coords.x = x + j;

                float4 pixel;

                // Read a pixel from the image.
                // Work on a channel
                pixel = read_imagef(sourceImage, sampler, coords);
                sum.x += pixel.x * filter[filterIdx++];
                //sum.y += pixel.y * filter[filterIdx++];
                //sum.z += pixel.z * filter[filterIdx++];
            }
        }

         barrier(CLK_GLOBAL_MEM_FENCE);
        // Copy the data to the output image if the
        // work-item is in bounds
        if(y < get_image_height(sourceImage) &&
                x < get_image_width(sourceImage))
        {
            coords.x = x;
            coords.y = y;

            //Same channel is copied in all three channels
            //write_imagef(outputImage, coords,
                        // (float4)(sum.x,sum.x,sum.x,1.0f));

            write_imagef(outputImage, coords, sum);
        }
    }
);

// This example shows how to read two images or use camera
// with OpenCV, transfer the frames to the GPU,
// and apply a convolution written in OpenCL
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

    //Filter Variables
    float filter[9] =  {
                -1.0,      0.0,      1.0,
                -2.0,      0.0,      2.0,
                -1.0,      0.0,      1.0,
                };

    // The convolution filter is 3x3
    int filterWidth = 3;

    //OpenCL variables
    // Get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    compute::buffer dev_filter(context, sizeof(filter),
                               compute::memory_object::read_only |
                               compute::memory_object::copy_host_ptr,
                               filter);

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
    return -1;
    }

    // create fliter kernel and set arguments
    compute::kernel filter_kernel(filter_program, "convolution");

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
    filter_kernel.set_arg(2, dev_filter);
    filter_kernel.set_arg(3, filterWidth);

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
        compute::opencv_imshow("Convoluted Image", dev_output_image, queue);

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
            compute::opencv_imshow("Convoluted Frame", dev_output_image, queue);

            // wait
            key = cv::waitKey(10);
        }
    }
    return 0;
}
