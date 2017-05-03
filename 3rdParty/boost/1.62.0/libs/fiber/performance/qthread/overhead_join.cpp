
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/atomic.hpp>
#include <boost/chrono.hpp>
#include <boost/cstdint.hpp>
#include <boost/preprocessor.hpp>
#include <boost/program_options.hpp>

#include <qthread/qthread.h>

#include "../clock.hpp"

#ifndef JOBS
#define JOBS BOOST_PP_LIMIT_REPEAT
#endif

boost::atomic< boost::uint64_t > counter( 0);

extern "C" aligned_t worker( void *)
{
    ++counter;
    return aligned_t();
}

duration_type measure( duration_type overhead)
{
    time_point_type start( clock_type::now() );
    for ( std::size_t i = 0; i < JOBS; ++i) {
        qthread_fork( & worker, 0, 0);
    }
    do
    {
        qthread_yield();
    } while ( counter != JOBS);
    duration_type total = clock_type::now() - start;
    total -= overhead_clock(); // overhead of measurement
    total /= JOBS;  // loops

    return total;
}

int main( int argc, char * argv[])
{
    try
    {
        boost::program_options::options_description desc("allowed options");
        desc.add_options()
            ("help", "help message");

        boost::program_options::variables_map vm;
        boost::program_options::store(
                boost::program_options::parse_command_line(
                    argc,
                    argv,
                    desc),
                vm);
        boost::program_options::notify( vm);

        if ( vm.count("help") ) {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        setenv("QT_NUM_SHEPHERDS", "1", 1);
        setenv("QT_NUM_WORKERS_PER_SHEPHERD", "1", 1);

        // Setup the qthreads environment.
        if ( 0 != qthread_initialize() )
            throw std::runtime_error("qthreads failed to initialize\n");

        duration_type overhead = overhead_clock();
        boost::uint64_t res = measure( overhead).count();
        std::cout << JOBS << " jobs: average of " << res << " nano seconds" << std::endl;

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
