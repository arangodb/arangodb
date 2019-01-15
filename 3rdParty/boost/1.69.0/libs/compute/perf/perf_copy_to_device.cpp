//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <vector>
#include <cstdlib>
#include <iostream>

#include <boost/compute.hpp>

int main(int argc, char *argv[])
{
    size_t size = 1000;
    if(argc >= 2){
        size = boost::lexical_cast<size_t>(argv[1]);
    }

    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);

    boost::compute::command_queue::properties
        properties = boost::compute::command_queue::enable_profiling;
    boost::compute::command_queue queue(context, device, properties);

    std::vector<int> host_vector(size);
    std::generate(host_vector.begin(), host_vector.end(), rand);

    boost::compute::vector<int> device_vector(host_vector.size(), context);

    boost::compute::future<void> future =
        boost::compute::copy_async(host_vector.begin(),
                                   host_vector.end(),
                                   device_vector.begin(),
                                   queue);

    // wait for copy to finish
    future.wait();

    // get elapsed time in nanoseconds
    size_t elapsed =
        future.get_event().duration<boost::chrono::nanoseconds>().count();

    std::cout << "time: " << elapsed / 1e6 << " ms" << std::endl;

    float rate = (float(size * sizeof(int)) / elapsed) * 1000.f;
    std::cout << "rate: " << rate << " MB/s" << std::endl;

    return 0;
}
