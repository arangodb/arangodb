//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/inclusive_scan.hpp>
#include <boost/compute/algorithm/inclusive_scan.hpp>
#include <boost/compute/interop/opencv/core.hpp>
#include <boost/compute/interop/opencv/highgui.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/uniform_real_distribution.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

// this example uses the random-number generation functions in Boost.Compute
// to calculate a large number of random "steps" and then plots the final
// random "walk" in a 2D image on the GPU and displays it with OpenCV
int main()
{
    // number of random steps to take
    size_t steps = 250000;

    // height and width of image
    size_t height = 800;
    size_t width = 800;

    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);

    using compute::int2_;

    // calaculate random values for each step
    compute::vector<float> random_values(steps, context);
    compute::default_random_engine random_engine(queue);
    compute::uniform_real_distribution<float> random_distribution(0.f, 4.f);

    random_distribution.generate(
        random_values.begin(), random_values.end(), random_engine, queue
    );

    // calaculate coordinates for each step
    compute::vector<int2_> coordinates(steps, context);

    // function to convert random values to random directions (in 2D)
    BOOST_COMPUTE_FUNCTION(int2_, take_step, (const float x),
    {
        if(x < 1.f){
            // move right
            return (int2)(1, 0);
        }
        if(x < 2.f){
            // move up
            return (int2)(0, 1);
        }
        if(x < 3.f){
            // move left
            return (int2)(-1, 0);
        }
        else {
            // move down
            return (int2)(0, -1);
        }
    });

    // transform the random values into random steps
    compute::transform(
        random_values.begin(), random_values.end(), coordinates.begin(), take_step, queue
    );

    // set staring position
    int2_ starting_position(width / 2, height / 2);
    compute::copy_n(&starting_position, 1, coordinates.begin(), queue);

    // scan steps to calculate position after each step
    compute::inclusive_scan(
        coordinates.begin(), coordinates.end(), coordinates.begin(), queue
    );

    // create output image
    compute::image2d image(
        context, width, height, compute::image_format(CL_RGBA, CL_UNSIGNED_INT8)
    );

    // program with two kernels, one to fill the image with white, and then
    // one the draw to points calculated in coordinates on the image
    const char draw_walk_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void draw_walk(__global const int2 *coordinates,
                                __write_only image2d_t image)
        {
            const uint i = get_global_id(0);
            const int2 coord = coordinates[i];

            if(coord.x > 0 && coord.x < get_image_width(image) &&
               coord.y > 0 && coord.y < get_image_height(image)){
                uint4 black = { 0, 0, 0, 0 };
                write_imageui(image, coord, black);
            }
        }

        __kernel void fill_white(__write_only image2d_t image)
        {
            const int2 coord = { get_global_id(0), get_global_id(1) };

            if(coord.x < get_image_width(image) &&
               coord.y < get_image_height(image)){
                uint4 white = { 255, 255, 255, 255 };
                write_imageui(image, coord, white);
            }
        }
    );

    // build the program
    compute::program draw_program =
        compute::program::build_with_source(draw_walk_source, context);

    // fill image with white
    compute::kernel fill_kernel(draw_program, "fill_white");
    fill_kernel.set_arg(0, image);

    const size_t offset[] = { 0, 0 };
    const size_t bounds[] = { width, height };

    queue.enqueue_nd_range_kernel(fill_kernel, 2, offset, bounds, 0);

    // draw random walk
    compute::kernel draw_kernel(draw_program, "draw_walk");
    draw_kernel.set_arg(0, coordinates);
    draw_kernel.set_arg(1, image);
    queue.enqueue_1d_range_kernel(draw_kernel, 0, coordinates.size(), 0);

    // show image
    compute::opencv_imshow("random walk", image, queue);

    // wait and return
    cv::waitKey(0);

    return 0;
}
