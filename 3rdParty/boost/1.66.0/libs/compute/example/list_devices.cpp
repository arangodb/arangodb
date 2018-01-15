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

int main()
{
    std::vector<compute::platform> platforms = compute::system::platforms();

    for(size_t i = 0; i < platforms.size(); i++){
        const compute::platform &platform = platforms[i];

        std::cout << "Platform '" << platform.name() << "'" << std::endl;

        std::vector<compute::device> devices = platform.devices();
        for(size_t j = 0; j < devices.size(); j++){
            const compute::device &device = devices[j];

            std::string type;
            if(device.type() & compute::device::gpu)
                type = "GPU Device";
            else if(device.type() & compute::device::cpu)
                type = "CPU Device";
            else if(device.type() & compute::device::accelerator)
                type = "Accelerator Device";
            else
                type = "Unknown Device";

            std::cout << "  " << type << ": " << device.name() << std::endl;
        }
    }

    return 0;
}
