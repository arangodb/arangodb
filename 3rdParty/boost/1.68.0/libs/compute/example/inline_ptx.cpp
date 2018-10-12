//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <algorithm>
#include <iostream>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>

namespace compute = boost::compute;

// this example shows how to embed PTX assembly instructions
// directly into boost.compute functions and use them with the
// transform() algorithm.
int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    // check to ensure we have an nvidia device
    if(gpu.vendor() != "NVIDIA Corporation"){
        std::cerr << "error: inline PTX assembly is only supported "
                  << "on NVIDIA devices."
                  << std::endl;
        return 0;
    }

    // create input values and copy them to the device
    using compute::uint_;
    uint_ data[] = { 0x00, 0x01, 0x11, 0xFF };
    compute::vector<uint_> input(data, data + 4, queue);

    // function returning the number of bits set (aka population count or
    // popcount) using the "popc" inline ptx assembly instruction.
    BOOST_COMPUTE_FUNCTION(uint_, nvidia_popc, (uint_ x),
    {
        uint count;
        asm("popc.b32 %0, %1;" : "=r"(count) : "r"(x));
        return count;
    });

    // calculate the popcount for each input value
    compute::vector<uint_> output(input.size(), context);
    compute::transform(
        input.begin(), input.end(), output.begin(), nvidia_popc, queue
    );

    // copy results back to the host and print them out
    std::vector<uint_> counts(output.size());
    compute::copy(output.begin(), output.end(), counts.begin(), queue);

    for(size_t i = 0; i < counts.size(); i++){
        std::cout << "0x" << std::hex << data[i]
                  << " has " << counts[i]
                  << " bits set" << std::endl;
    }

    return 0;
}
