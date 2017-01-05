//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#include <boost/compute/function.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/types/fundamental.hpp>

namespace compute = boost::compute;

int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);

    std::cout << "device: " << gpu.name() << std::endl;

    using compute::uint_;
    using compute::uint2_;

    // ten million random points
    size_t n = 10000000;

    // generate random numbers
    compute::default_random_engine rng(queue);
    compute::vector<uint_> vector(n * 2, context);
    rng.generate(vector.begin(), vector.end(), queue);

    // function returing true if the point is within the unit circle
    BOOST_COMPUTE_FUNCTION(bool, is_in_unit_circle, (const uint2_ point),
    {
        const float x = point.x / (float) UINT_MAX - 1;
        const float y = point.y / (float) UINT_MAX - 1;

        return (x*x + y*y) < 1.0f;
    });

    // iterate over vector<uint> as vector<uint2>
    compute::buffer_iterator<uint2_> start =
        compute::make_buffer_iterator<uint2_>(vector.get_buffer(), 0);
    compute::buffer_iterator<uint2_> end =
        compute::make_buffer_iterator<uint2_>(vector.get_buffer(), vector.size() / 2);

    // count number of random points within the unit circle
    size_t count = compute::count_if(start, end, is_in_unit_circle, queue);

    // print out values
    float count_f = static_cast<float>(count);
    std::cout << "count: " << count << " / " << n << std::endl;
    std::cout << "ratio: " << count_f / float(n) << std::endl;
    std::cout << "pi = " << (count_f / float(n)) * 4.0f << std::endl;

    return 0;
}
