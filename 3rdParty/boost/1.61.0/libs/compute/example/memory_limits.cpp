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

#include <boost/compute/core.hpp>

namespace compute = boost::compute;

int main()
{
    // get the default device
    compute::device device = compute::system::default_device();

    std::cout << "device: " << device.name() << std::endl;
    std::cout << "  global memory size: "
              << device.get_info<cl_ulong>(CL_DEVICE_GLOBAL_MEM_SIZE) / 1024 / 1024
              << " MB"
              << std::endl;
    std::cout << "  local memory size: "
              << device.get_info<cl_ulong>(CL_DEVICE_LOCAL_MEM_SIZE) / 1024
              << " KB"
              << std::endl;
    std::cout << "  constant memory size: "
              << device.get_info<cl_ulong>(CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE) / 1024
              << " KB"
              << std::endl;

    return 0;
}
