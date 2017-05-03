//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAmdCppKernel
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/kernel.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/detail/vendor.hpp>
#include <boost/compute/utility/source.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(amd_template_function)
{
    if(!compute::detail::is_amd_device(device)){
        std::cerr << "skipping amd_template_function test: c++ static kernel "
                     "language is only supported on AMD devices." << std::endl;
        return;
    }

    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        template<typename T>
        inline T square(const T x)
        {
            return x * x;
        }

        template<typename T>
        __kernel void square_kernel(__global T *data)
        {
            const uint i = get_global_id(0);
            data[i] = square(data[i]);
        }

        template __attribute__((mangled_name(square_kernel_int)))
        __kernel void square_kernel(__global int *data);
    );

    int data[] = { 1, 2, 3, 4 };
    compute::vector<int> vec(data, data + 4, queue);

    compute::program square_program =
        compute::program::build_with_source(source, context, "-x clc++");

    compute::kernel square_kernel(square_program, "square_kernel_int");
    square_kernel.set_arg(0, vec);

    queue.enqueue_1d_range_kernel(square_kernel, 0, vec.size(), 4);

    CHECK_RANGE_EQUAL(int, 4, vec, (1, 4, 9, 16));
}

BOOST_AUTO_TEST_SUITE_END()
