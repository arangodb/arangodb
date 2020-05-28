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

#include <boost/compute/core.hpp>

namespace compute = boost::compute;

// this example demonstrates how to use the Boost.Compute classes to
// setup and run a simple vector addition kernel on the GPU
int main()
{
    // get the default device
    compute::device device = compute::system::default_device();

    // create a context for the device
    compute::context context(device);

    // setup input arrays
    float a[] = { 1, 2, 3, 4 };
    float b[] = { 5, 6, 7, 8 };

    // make space for the output
    float c[] = { 0, 0, 0, 0 };

    // create memory buffers for the input and output
    compute::buffer buffer_a(context, 4 * sizeof(float));
    compute::buffer buffer_b(context, 4 * sizeof(float));
    compute::buffer buffer_c(context, 4 * sizeof(float));

    // source code for the add kernel
    const char source[] =
        "__kernel void add(__global const float *a,"
        "                  __global const float *b,"
        "                  __global float *c)"
        "{"
        "    const uint i = get_global_id(0);"
        "    c[i] = a[i] + b[i];"
        "}";

    // create the program with the source
    compute::program program =
        compute::program::create_with_source(source, context);

    // compile the program
    program.build();

    // create the kernel
    compute::kernel kernel(program, "add");

    // set the kernel arguments
    kernel.set_arg(0, buffer_a);
    kernel.set_arg(1, buffer_b);
    kernel.set_arg(2, buffer_c);

    // create a command queue
    compute::command_queue queue(context, device);

    // write the data from 'a' and 'b' to the device
    queue.enqueue_write_buffer(buffer_a, 0, 4 * sizeof(float), a);
    queue.enqueue_write_buffer(buffer_b, 0, 4 * sizeof(float), b);

    // run the add kernel
    queue.enqueue_1d_range_kernel(kernel, 0, 4, 0);

    // transfer results back to the host array 'c'
    queue.enqueue_read_buffer(buffer_c, 0, 4 * sizeof(float), c);

    // print out results in 'c'
    std::cout << "c: [" << c[0] << ", "
                        << c[1] << ", "
                        << c[2] << ", "
                        << c[3] << "]" << std::endl;

    return 0;
}
