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

#include <boost/program_options.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

namespace po = boost::program_options;
namespace compute = boost::compute;

template<class T>
double perf_sort(const std::vector<T>& data,
                 const size_t trials,
                 compute::command_queue& queue)
{
    compute::vector<T> vec(data.size(), queue.get_context());

    perf_timer t;
    for(size_t trial = 0; trial < trials; trial++){
        compute::copy(data.begin(), data.end(), vec.begin(), queue);
        t.start();
        compute::sort(vec.begin(), vec.end(), queue);
        queue.finish();
        t.stop();

        if(!compute::is_sorted(vec.begin(), vec.end(), queue)){
            std::cerr << "ERROR: is_sorted() returned false" << std::endl;
        }
    }
    return t.min_time();
}

template<class T>
void tune_sort(const std::vector<T>& data,
               const size_t trials,
               compute::command_queue& queue)
{
    boost::shared_ptr<compute::detail::parameter_cache>
        params = compute::detail::parameter_cache::get_global_cache(queue.get_device());

    const std::string cache_key =
        std::string("__boost_radix_sort_") + compute::type_name<T>();

    const compute::uint_ tpbs[] = { 32, 64, 128, 256, 512, 1024 };

    double min_time = (std::numeric_limits<double>::max)();
    compute::uint_ best_tpb = 0;

    for(size_t i = 0; i < sizeof(tpbs) / sizeof(*tpbs); i++){
        params->set(cache_key, "tpb", tpbs[i]);

        try {
            const double t = perf_sort(data, trials, queue);
            if(t < min_time){
                best_tpb = tpbs[i];
                min_time = t;
            }
        }
        catch(compute::opencl_error&){
            // invalid work group size for this device, skip
        }
    }

    // store optimal parameters
    params->set(cache_key, "tpb", best_tpb);
}

int main(int argc, char *argv[])
{
    // setup command line arguments
    po::options_description options("options");
    options.add_options()
        ("help", "show usage instructions")
        ("size", po::value<size_t>()->default_value(8192), "input size")
        ("trials", po::value<size_t>()->default_value(3), "number of trials to run")
        ("tune", "run tuning procedure")
    ;
    po::positional_options_description positional_options;
    positional_options.add("size", 1);

    // parse command line
    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv)
            .options(options).positional(positional_options).run(),
        vm
    );
    po::notify(vm);

    const size_t size = vm["size"].as<size_t>();
    const size_t trials = vm["trials"].as<size_t>();
    std::cout << "size: " << size << std::endl;

    // setup context and queue for the default device
    compute::device device = boost::compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    // create vector of random numbers on the host
    std::vector<unsigned int> data(size);
    std::generate(data.begin(), data.end(), rand);

    // run tuning proceure (if requested)
    if(vm.count("tune")){
        tune_sort(data, trials, queue);
    }

    // run sort benchmark
    double t = perf_sort(data, trials, queue);
    std::cout << "time: " << t / 1e6 << " ms" << std::endl;

    return 0;
}
