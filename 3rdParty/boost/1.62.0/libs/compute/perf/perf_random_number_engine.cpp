//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <vector>

#include <boost/program_options.hpp>

#include <boost/compute/container/vector.hpp>
#include <boost/compute/core.hpp>
#include <boost/compute/random.hpp>

#include "perf.hpp"

namespace compute = boost::compute;
namespace po = boost::program_options;

template<class Engine>
void perf_random_number_engine(const size_t size,
                               const size_t trials,
                               compute::command_queue& queue)
{
    typedef typename Engine::result_type T;

    // create random number engine
    Engine engine(queue);

    // create vector on the device
    std::cout << "size = " << size << std::endl;
    compute::vector<T> vector(size, queue.get_context());

    // generate random numbers
    perf_timer t;
    for(size_t i = 0; i < trials; i++){
        t.start();
        engine.generate(vector.begin(), vector.end(), queue);
        queue.finish();
        t.stop();
    }

    // print result
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "rate: " << perf_rate<T>(size, t.min_time()) << " MB/s" << std::endl;
}

int main(int argc, char *argv[])
{
    // setup and parse command line options
    po::options_description options("options");
    options.add_options()
        ("help", "show usage instructions")
        ("size", po::value<size_t>()->default_value(8192), "number of values")
        ("trials", po::value<size_t>()->default_value(3), "number of trials")
        ("engine", po::value<std::string>()->default_value("default_random_engine"), "random number engine")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << options << std::endl;
        return 0;
    }

    // setup context and queue for the default device
    compute::device device = compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);

    // get command line options
    const size_t size = vm["size"].as<size_t>();
    const size_t trials = vm["trials"].as<size_t>();
    const std::string& engine = vm["engine"].as<std::string>();

    // run benchmark
    if(engine == "default_random_engine"){
        perf_random_number_engine<compute::default_random_engine>(size, trials, queue);
    }
    else if(engine == "mersenne_twister_engine"){
        perf_random_number_engine<compute::mt19937>(size, trials, queue);
    }
    else if(engine == "linear_congruential_engine"){
        perf_random_number_engine<compute::linear_congruential_engine<> >(size, trials, queue);
    }
    else if(engine == "threefry_engine"){
        perf_random_number_engine<compute::threefry_engine<> >(size, trials, queue);
    }
    else {
        std::cerr << "error: unknown random number engine '" << engine << "'" << std::endl;
        return -1;
    }

    return 0;
}
