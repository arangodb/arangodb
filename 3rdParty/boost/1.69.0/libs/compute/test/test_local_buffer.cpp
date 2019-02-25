//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestLocalBuffer
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/core.hpp>
#include <boost/compute/memory/local_buffer.hpp>
#include <boost/compute/utility/source.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(local_buffer_arg)
{
    if(device.get_info<CL_DEVICE_LOCAL_MEM_TYPE>() != CL_LOCAL){
        std::cerr << "skipping local buffer arg test: "
                  << "device does not support local memory" << std::endl;
        return;
    }

    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void foo(__local float *local_buffer,
                          __global float *global_buffer)
        {
            const uint gid = get_global_id(0);
            const uint lid = get_local_id(0);

            local_buffer[lid] = gid;

            global_buffer[gid] = local_buffer[lid];
        }
    );

    // create and build program
    compute::program program =
        compute::program::build_with_source(source, context);

    // create kernel
    compute::kernel kernel = program.create_kernel("foo");

    // setup kernel arguments
    compute::buffer global_buf(context, 128 * sizeof(float));
    kernel.set_arg(0, compute::local_buffer<float>(32));
    kernel.set_arg(1, global_buf);

    // some implementations don't correctly report dynamically-set local buffer sizes
    if(kernel.get_work_group_info<cl_ulong>(device, CL_KERNEL_LOCAL_MEM_SIZE) == 0){
        std::cerr << "skipping checks for local memory size, device reports "
                  << "zero after setting dynamically-sized local buffer size"
                  << std::endl;
        return;
    }

    // check actual memory size
    BOOST_CHECK_GE(
        kernel.get_work_group_info<cl_ulong>(device, CL_KERNEL_LOCAL_MEM_SIZE),
        32 * sizeof(float)
    );

    // increase local buffer size and check new actual local memory size
    kernel.set_arg(0, compute::local_buffer<float>(64));

    BOOST_CHECK_GE(
        kernel.get_work_group_info<cl_ulong>(device, CL_KERNEL_LOCAL_MEM_SIZE),
        64 * sizeof(float)
    );
}

BOOST_AUTO_TEST_SUITE_END()
