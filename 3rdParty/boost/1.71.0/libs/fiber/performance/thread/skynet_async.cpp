
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// based on https://github.com/atemerev/skynet from Alexander Temerev 

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#include "buffered_channel.hpp"

using channel_type = buffered_channel< std::uint64_t >;
using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;

// microbenchmark
std::uint64_t skynet( std::uint64_t num, std::uint64_t size, std::uint64_t div)
{
    if ( size != 1){
        size /= div;

        std::vector<std::future<std::uint64_t> > results;
        results.reserve( div);

        for ( std::uint64_t i = 0; i != div; ++i) {
            std::uint64_t sub_num = num + i * size;
            results.emplace_back(
                std::async( skynet, sub_num, size, div) );
        }

        std::uint64_t sum = 0;
        for ( auto& f : results)
            sum += f.get();
            
        return sum;
    }

    return num;
}

int main() {
    try {
        std::size_t size{ 10000 };
        std::size_t div{ 10 };
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        time_point_type start{ clock_type::now() };
        result = skynet( 0, size, div);
        duration = clock_type::now() - start;
        std::cout << "Result: " << result << " in " << duration.count() / 1000000 << " ms" << std::endl;
        std::cout << "done." << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
