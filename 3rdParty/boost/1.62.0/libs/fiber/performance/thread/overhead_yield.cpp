
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <boost/cstdint.hpp>
#include <boost/preprocessor.hpp>

#include "../clock.hpp"

#ifndef JOBS
#define JOBS BOOST_PP_LIMIT_REPEAT
#endif

#define JOIN(z, n, _) \
    std::thread( worker, overhead, & result).join();

void worker( duration_type overhead, duration_type * result)
{
    time_point_type start( clock_type::now() );
    std::this_thread::yield();
    duration_type total = clock_type::now() - start;
    total -= overhead;
    * result += total;
}

duration_type measure( duration_type overhead)
{
    duration_type result = duration_type::zero();

    std::thread( worker, overhead, & result).join();

    result = duration_type::zero();

    BOOST_PP_REPEAT_FROM_TO(1, JOBS, JOIN, _)

    result /= JOBS;  // loops

    return result;
}

int main( int argc, char * argv[])
{
    try
    {
        duration_type overhead = overhead_clock();
        boost::uint64_t res = measure( overhead).count();
        std::cout << "average of " << res << " nano seconds" << std::endl;

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
