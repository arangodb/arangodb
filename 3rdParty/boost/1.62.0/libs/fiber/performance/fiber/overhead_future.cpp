
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <boost/cstdint.hpp>
#include <boost/fiber/all.hpp>
#include <boost/preprocessor.hpp>

#include "../clock.hpp"

#ifndef JOBS
#define JOBS BOOST_PP_LIMIT_REPEAT
#endif

#define WAIT(z, n, _) { \
    boost::fibers::packaged_task< void() > pt( worker); \
    boost::fibers::future< void > f( pt.get_future() ); \
    boost::fibers::fiber( std::move( pt) ).detach(); \
    time_point_type start( clock_type::now() ); \
    f.wait(); \
    duration_type total = clock_type::now() - start; \
    total -= overhead; \
    result += total; \
}

void worker() {}

duration_type measure( duration_type overhead) {
    boost::fibers::fiber( worker).join();
    duration_type result = duration_type::zero();
    BOOST_PP_REPEAT_FROM_TO(1, JOBS, WAIT, _)
    result /= JOBS;  // loops
    return result;
}

int main( int argc, char * argv[]) {
    try {
        duration_type overhead = overhead_clock();
        boost::uint64_t res = measure( overhead).count();
        std::cout << "average of " << res << " nano seconds" << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
    return EXIT_FAILURE;
}
