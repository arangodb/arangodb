//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <boost/compute/core.hpp>
#include <boost/compute/closure.hpp>
#include <boost/compute/algorithm/copy_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/uniform_int_distribution.hpp>
#include <boost/compute/random/uniform_real_distribution.hpp>

#include "perf.hpp"

namespace compute = boost::compute;

void test_copy_if_odd(compute::command_queue &queue)
{
    // create input and output vectors on the device
    const compute::context &context = queue.get_context();
    compute::vector<int> input(PERF_N, context);
    compute::vector<int> output(PERF_N, context);

    // generate random numbers between 1 and 10
    compute::default_random_engine rng(queue);
    compute::uniform_int_distribution<int> d(1, 10);
    d.generate(input.begin(), input.end(), rng, queue);

    BOOST_COMPUTE_FUNCTION(bool, is_odd, (int x),
    {
        return x & 1;
    });

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        compute::vector<int>::iterator i = compute::copy_if(
            input.begin(), input.end(), output.begin(), is_odd, queue
        );
        queue.finish();
        t.stop();

        float ratio = float(std::distance(output.begin(), i)) / PERF_N;
        if(PERF_N > 1000 && (ratio < 0.45f || ratio > 0.55f)){
            std::cerr << "error: ratio is " << ratio << std::endl;
            std::cerr << "error: ratio should be around 45-55%" << std::endl;
        }
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
}

void test_copy_if_in_sphere(compute::command_queue &queue)
{
    using boost::compute::float4_;

    // create input and output vectors on the device
    const compute::context &context = queue.get_context();
    compute::vector<float4_> input_points(PERF_N, context);
    compute::vector<float4_> output_points(PERF_N, context);

    // generate random numbers in a cube
    float radius = 5.0f;
    compute::default_random_engine rng(queue);
    compute::uniform_real_distribution<float> d(-radius, +radius);
    d.generate(
        compute::make_buffer_iterator<float>(input_points.get_buffer(), 0),
        compute::make_buffer_iterator<float>(input_points.get_buffer(), PERF_N * 4),
        rng,
        queue
    );

    // predicate which returns true if the point lies within the sphere
    BOOST_COMPUTE_CLOSURE(bool, is_in_sphere, (float4_ point), (radius),
    {
        // ignore fourth component
        point.w = 0;

        return length(point) < radius;
    });

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        compute::vector<float4_>::iterator i = compute::copy_if(
            input_points.begin(),
            input_points.end(),
            output_points.begin(),
            is_in_sphere,
            queue
        );
        queue.finish();
        t.stop();

        float ratio = float(std::distance(output_points.begin(), i)) / PERF_N;
        if(PERF_N > 1000 && (ratio < 0.5f || ratio > 0.6f)){
            std::cerr << "error: ratio is " << ratio << std::endl;
            std::cerr << "error: ratio should be around 50-60%" << std::endl;
        }
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    // setup context and queue for the default device
    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    test_copy_if_odd(queue);

    return 0;
}
