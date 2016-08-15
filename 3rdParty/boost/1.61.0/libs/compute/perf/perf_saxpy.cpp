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

#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

namespace po = boost::program_options;
namespace compute = boost::compute;

float rand_float()
{
    return (float(rand()) / float(RAND_MAX)) * 1000.f;
}

template<class T>
double perf_saxpy(const compute::vector<T>& x,
                  const compute::vector<T>& y,
                  const T alpha,
                  const size_t trials,
                  compute::command_queue& queue)
{
    // create vector on the device to store the result
    compute::vector<T> result(x.size(), queue.get_context());

    perf_timer t;
    for(size_t trial = 0; trial < trials; trial++){
        compute::fill(result.begin(), result.end(), T(0), queue);

        t.start();

        using compute::lambda::_1;
        using compute::lambda::_2;

        compute::transform(
            x.begin(), x.end(), y.begin(), result.begin(), alpha * _1 + _2, queue
        );

        queue.finish();
        t.stop();
    }

    return t.min_time();
}

template<class T>
void tune_saxpy(const compute::vector<T>& x,
                const compute::vector<T>& y,
                const T alpha,
                const size_t trials,
                compute::command_queue& queue)
{
    boost::shared_ptr<compute::detail::parameter_cache>
        params = compute::detail::parameter_cache::get_global_cache(queue.get_device());

    const std::string cache_key =
        std::string("__boost_copy_kernel_") + boost::lexical_cast<std::string>(sizeof(T));

    const compute::uint_ tpbs[] = { 4, 8, 16, 32, 64, 128, 256, 512, 1024 };
    const compute::uint_ vpts[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

    double min_time = (std::numeric_limits<double>::max)();
    compute::uint_ best_tpb = 0;
    compute::uint_ best_vpt = 0;

    for(size_t i = 0; i < sizeof(tpbs) / sizeof(*tpbs); i++){
        params->set(cache_key, "tpb", tpbs[i]);
        for(size_t j = 0; j < sizeof(vpts) / sizeof(*vpts); j++){
            params->set(cache_key, "vpt", vpts[j]);

            try {
                const double t = perf_saxpy(x, y, alpha, trials, queue);
                if(t < min_time){
                    best_tpb = tpbs[i];
                    best_vpt = vpts[j];
                    min_time = t;
                }
            }
            catch(compute::opencl_error&){
                // invalid parameters for this device, skip
            }
        }
    }

    // store optimal parameters
    params->set(cache_key, "tpb", best_tpb);
    params->set(cache_key, "vpt", best_vpt);
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
        ("alpha", po::value<double>()->default_value(2.5), "saxpy alpha value")
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
    const float alpha = vm["alpha"].as<double>();
    std::cout << "size: " << size << std::endl;

    // setup context and queue for the default device
    compute::device device = boost::compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    // create vector of random numbers on the host
    std::vector<float> host_x(size);
    std::vector<float> host_y(size);
    std::generate(host_x.begin(), host_x.end(), rand_float);
    std::generate(host_y.begin(), host_y.end(), rand_float);

    // create vector on the device and copy the data
    compute::vector<float> x(host_x.begin(), host_x.end(), queue);
    compute::vector<float> y(host_y.begin(), host_y.end(), queue);

    // run tuning proceure (if requested)
    if(vm.count("tune")){
        tune_saxpy(x, y, alpha, trials, queue);
    }

    // run benchmark
    double t = perf_saxpy(x, y, alpha, trials, queue);
    std::cout << "time: " << t / 1e6 << " ms" << std::endl;

    return 0;
}
