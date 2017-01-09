
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

#define CREATE(z, n, _) \
    boost::fibers::fiber BOOST_PP_CAT(f,n) (worker);

#define DETACH(z, n, _) \
    BOOST_PP_CAT(f,n) .detach();

void worker() {}

duration_type measure( duration_type overhead) {
    boost::fibers::fiber( worker).join();
    BOOST_PP_REPEAT_FROM_TO(1, JOBS, CREATE, _)
    time_point_type start( clock_type::now() );
    BOOST_PP_REPEAT_FROM_TO(1, JOBS, DETACH, _);
    duration_type result = clock_type::now() - start;
    result -= overhead;
    result /= JOBS;  // joined fibers
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
