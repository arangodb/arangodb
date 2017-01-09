
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

#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

#include "../clock.hpp"

#ifndef JOBS
#define JOBS BOOST_PP_LIMIT_REPEAT
#endif

struct worker: public tbb::task
{
    tbb::task * execute()
    { return 0; }
};

struct spawner : public tbb::task
{
    tbb::task * execute()
    {
        set_ref_count( static_cast< int >( JOBS + 1) );

        for ( boost::uint64_t i = 0; i < JOBS; ++i)
        {
            worker & wrk = *new ( tbb::task::allocate_child() ) worker();

            if ( i == ( JOBS - 1) )
                spawn_and_wait_for_all( wrk);
            else
                spawn( wrk);
        }

        return 0;
    }
};

duration_type measure( duration_type overhead)
{
    time_point_type start( clock_type::now() );
    spawner & spw = *new ( tbb::task::allocate_root() ) spawner();
    tbb::task::spawn_root_and_wait( spw);
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

        tbb::task_scheduler_init init( 1);

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
