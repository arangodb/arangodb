//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Mageswaran.D <mageswaran1989@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

//Code sample for calculating histogram using OpenCL and
//displaying image histogram in OpenCV.

#define BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION

#include <iostream>
#include <string>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/interop/opencv/core.hpp>
#include <boost/compute/interop/opencv/highgui.hpp>
#include <boost/program_options.hpp>

namespace compute = boost::compute;
namespace po = boost::program_options;

// number of bins
int histSize = 256;

// Set the ranges ( for B,G,R) )
// TryOut: consider the range in kernel calculation
float range[] = { 0, 256 } ;
const float* histRange = { range };

// Create naive histogram program
// Needs "cl_khr_local_int32_base_atomics" extension
const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE (
    __kernel void histogram(read_only image2d_t src_image,
                            __global int* b_hist,
                            __global int* g_hist,
                            __global int* r_hist)
            {
                sampler_t sampler =( CLK_NORMALIZED_COORDS_FALSE |
                                     CLK_FILTER_NEAREST |
                                     CLK_ADDRESS_CLAMP_TO_EDGE);

                int image_width = get_image_width(src_image);
                int image_height = get_image_height(src_image);

                int2 coords  = (int2)(get_global_id(0), get_global_id(1));
                float4 pixel = read_imagef(src_image,sampler, coords);

                //boundary condition
                if ((coords.x < image_width) && (coords.y < image_height))
                {
                    uchar indx_x, indx_y, indx_z;
                    indx_x = convert_uchar_sat(pixel.x * 255.0f);
                    indx_y = convert_uchar_sat(pixel.y * 255.0f);
                    indx_z = convert_uchar_sat(pixel.z * 255.0f);

                    atomic_inc(&b_hist[(uint)indx_z]);
                    atomic_inc(&g_hist[(uint)indx_y]);
                    atomic_inc(&r_hist[(uint)indx_x]);
                }
            }
);

inline void showHistogramWindow(cv::Mat &b_hist, cv::Mat &g_hist, cv::Mat &r_hist,
                         std::string window_name)
{
    // Draw the histograms for B, G and R
    int hist_w = 1024;
    int hist_h = 768;
    int bin_w = cvRound((double)hist_w/histSize);

    cv::Mat histImage(hist_h, hist_w, CV_8UC3, cv::Scalar(0,0,0));

    // Normalize the result to [ 0, histImage.rows ]
    cv::normalize(b_hist, b_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());
    cv::normalize(g_hist, g_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());
    cv::normalize(r_hist, r_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());

    // Draw for each channel
    for (int i = 1; i < histSize; i++ )
    {
        cv::line(histImage,
                 cv::Point(bin_w*(i-1), hist_h - cvRound(b_hist.at<float>(i-1))),
                 cv::Point(bin_w*(i), hist_h - cvRound(b_hist.at<float>(i))),
                 cv::Scalar(255, 0, 0),
                 2,
                 8,
                 0);

        cv::line(histImage,
                 cv::Point(bin_w*(i-1), hist_h - cvRound(g_hist.at<float>(i-1))),
                 cv::Point(bin_w*(i), hist_h - cvRound(g_hist.at<float>(i))),
                 cv::Scalar(0, 255, 0),
                 2,
                 8,
                 0);

        cv::line(histImage,
                 cv::Point( bin_w*(i-1), hist_h - cvRound(r_hist.at<float>(i-1))),
                 cv::Point( bin_w*(i), hist_h - cvRound(r_hist.at<float>(i)) ),
                 cv::Scalar( 0, 0, 255),
                 2,
                 8,
                 0);
    }

    // Display
    cv::namedWindow(window_name, CV_WINDOW_AUTOSIZE );
    cv::imshow(window_name, histImage );
}

//Get the device context
//Create GPU array/vector
//Copy the image & set up the kernel
//Execute the kernel
//Copy GPU data back to CPU cv::Mat data pointer
//OpenCV conversion for convienient display
void calculateHistogramUsingCL(cv::Mat src, compute::command_queue &queue)
{
    compute::context context = queue.get_context();

    // Convert image to BGRA (OpenCL requires 16-byte aligned data)
    cv::cvtColor(src, src, CV_BGR2BGRA);

    //3 channels & 256 bins : alpha channel is ignored
    compute::vector<int> gpu_b_hist(histSize, context);
    compute::vector<int> gpu_g_hist(histSize, context);
    compute::vector<int> gpu_r_hist(histSize, context);

    // Transfer image to gpu
    compute::image2d gpu_src =
            compute::opencv_create_image2d_with_mat(
                src, compute::image2d::read_only,
                queue
                );

    compute::program histogram_program =
            compute::program::create_with_source(source, context);
    histogram_program.build();

    // create histogram kernel and set arguments
    compute::kernel histogram_kernel(histogram_program, "histogram");
    histogram_kernel.set_arg(0, gpu_src);
    histogram_kernel.set_arg(1, gpu_b_hist.get_buffer());
    histogram_kernel.set_arg(2, gpu_g_hist.get_buffer());
    histogram_kernel.set_arg(3, gpu_r_hist.get_buffer());

    // run histogram kernel
    // each kernel thread updating red, green & blue bins
    size_t origin[2] = { 0, 0 };
    size_t region[2] = { gpu_src.width(),
                         gpu_src.height() };

    queue.enqueue_nd_range_kernel(histogram_kernel, 2, origin, region, 0);

    //Make sure kernel get executed and data copied back
    queue.finish();

    //create Mat and copy GPU bins to CPU memory
    cv::Mat b_hist(256, 1, CV_32SC1);
    compute::copy(gpu_b_hist.begin(), gpu_b_hist.end(), b_hist.data, queue);
    cv::Mat g_hist(256, 1, CV_32SC1);
    compute::copy(gpu_g_hist.begin(), gpu_g_hist.end(), g_hist.data, queue);
    cv::Mat r_hist(256, 1, CV_32SC1);
    compute::copy(gpu_r_hist.begin(), gpu_r_hist.end(), r_hist.data, queue);

    b_hist.convertTo(b_hist, CV_32FC1); //converted for displaying
    g_hist.convertTo(g_hist, CV_32FC1);
    r_hist.convertTo(r_hist, CV_32FC1);

    showHistogramWindow(b_hist, g_hist, r_hist, "Histogram");
}

int main( int argc, char** argv )
{
    // Get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);

    cv::Mat src;

    // setup the command line arguments
    po::options_description desc;
    desc.add_options()
            ("help",  "show available options")
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

    //check for image paths
    if(vm.count("image"))
    {
        // Read image with OpenCV
        src = cv::imread(vm["image"].as<std::string>(),
                                       CV_LOAD_IMAGE_COLOR);
        if(!src.data){
            std::cerr << "Failed to load image" << std::endl;
            return -1;
        }
        calculateHistogramUsingCL(src, queue);
        cv::imshow("Image", src);
        cv::waitKey(0);
    }
    else
    {
        std::cout << desc << std::endl;
        return 0;
    }
    return 0;
}
