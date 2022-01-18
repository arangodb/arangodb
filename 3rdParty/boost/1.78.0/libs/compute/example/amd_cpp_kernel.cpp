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

#include <boost/compute/command_queue.hpp>
#include <boost/compute/kernel.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

// this example shows how to use the static c++ kernel language
// extension (currently only supported by AMD) to compile and
// execute a templated c++ kernel.
// Using platform vendor info to decide if this is AMD platform
int main()
{
    // get default device and setup context
    compute::device device = compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);

    // check the platform vendor string
    if(device.platform().vendor() != "Advanced Micro Devices, Inc."){
        std::cerr << "error: static C++ kernel language is only "
                  << "supported on AMD devices."
                  << std::endl;
        return 0;
    }

    // create input int values and copy them to the device
    int int_data[] = { 1, 2, 3, 4};
    compute::vector<int> int_vector(int_data, int_data + 4, queue);

    // create input float values and copy them to the device
    float float_data[] = { 2.0f, 4.0f, 6.0f, 8.0f };
    compute::vector<float> float_vector(float_data, float_data + 4, queue);

    // create kernel source with a templated function and templated kernel
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        // define our templated function which returns the square of its input
        template<typename T>
        inline T square(const T x)
        {
            return x * x;
        }

        // define our templated kernel which calls square on each value in data
        template<typename T>
        __kernel void square_kernel(__global T *data)
        {
            const uint i = get_global_id(0);
            data[i] = square(data[i]);
        }

        // explicitly instantiate the square kernel for int's. this allows
        // for it to be called from the host with the given mangled name.
        template __attribute__((mangled_name(square_kernel_int)))
        __kernel void square_kernel(__global int *data);

        // also instantiate the square kernel for float's.
        template __attribute__((mangled_name(square_kernel_float)))
        __kernel void square_kernel(__global float *data);
    );

    // build the program. must enable the c++ static kernel language
    // by passing the "-x clc++" compile option.
    compute::program square_program =
        compute::program::build_with_source(source, context, "-x clc++");

    // create the square kernel for int's by using its mangled name declared
    // in the explicit template instantiation.
    compute::kernel square_int_kernel(square_program, "square_kernel_int");
    square_int_kernel.set_arg(0, int_vector);

    // execute the square int kernel
    queue.enqueue_1d_range_kernel(square_int_kernel, 0, int_vector.size(), 4);

    // print out the squared int values
    std::cout << "int's: ";
    compute::copy(
        int_vector.begin(), int_vector.end(),
        std::ostream_iterator<int>(std::cout, " "),
        queue
    );
    std::cout << std::endl;

    // now create the square kernel for float's
    compute::kernel square_float_kernel(square_program, "square_kernel_float");
    square_float_kernel.set_arg(0, float_vector);

    // execute the square int kernel
    queue.enqueue_1d_range_kernel(square_float_kernel, 0, float_vector.size(), 4);

    // print out the squared float values
    std::cout << "float's: ";
    compute::copy(
        float_vector.begin(), float_vector.end(),
        std::ostream_iterator<float>(std::cout, " "),
        queue
    );
    std::cout << std::endl;

    return 0;
}
