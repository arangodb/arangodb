//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

//[point_centroid_example

#include <iostream>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/accumulate.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

namespace compute = boost::compute;

// the point centroid example calculates and displays the
// centroid of a set of 3D points stored as float4's
int main()
{
    using compute::float4_;

    // get default device and setup context
    compute::device device = compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);

    // point coordinates
    float points[] = { 1.0f, 2.0f, 3.0f, 0.0f,
                       -2.0f, -3.0f, 4.0f, 0.0f,
                       1.0f, -2.0f, 2.5f, 0.0f,
                       -7.0f, -3.0f, -2.0f, 0.0f,
                       3.0f, 4.0f, -5.0f, 0.0f };

    // create vector for five points
    compute::vector<float4_> vector(5, context);

    // copy point data to the device
    compute::copy(
        reinterpret_cast<float4_ *>(points),
        reinterpret_cast<float4_ *>(points) + 5,
        vector.begin(),
        queue
    );

    // calculate sum
    float4_ sum = compute::accumulate(
        vector.begin(), vector.end(), float4_(0, 0, 0, 0), queue
    );

    // calculate centroid
    float4_ centroid;
    for(size_t i = 0; i < 3; i++){
        centroid[i] = sum[i] / 5.0f;
    }

    // print centroid
    std::cout << "centroid: " << centroid << std::endl;

    return 0;
}

//]
