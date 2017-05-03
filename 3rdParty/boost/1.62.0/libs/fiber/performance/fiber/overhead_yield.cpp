
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
    boost::fibers::fiber BOOST_PP_CAT(f,n) ( \
            [&total,&overhead](){ \
                time_point_type start( clock_type::now() ); \
                boost::this_fiber::yield(); \
                duration_type diff = clock_type::now() - start; \
                diff -= overhead; \
                total += diff; \
            }); 

#define JOIN(z, n, _) \
    BOOST_PP_CAT(f,n) .join();

void worker() {}

duration_type measure( duration_type overhead) {
    boost::fibers::fiber( worker).join();
    duration_type total = duration_type::zero();
    BOOST_PP_REPEAT_FROM_TO(1, JOBS, CREATE, _)
    BOOST_PP_REPEAT_FROM_TO(1, JOBS, JOIN, _);
    total /=  JOBS;  // JOBS switches for return from this_fiber::yield()
    total /=  JOBS;  // measurement of JOBS fibers 
    return total;
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
