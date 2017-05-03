
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <boost/cstdint.hpp>
#include <boost/program_options.hpp>

#include <ucontext.h>

#include "../bind_processor.hpp"
#include "../clock.hpp"
#include "../cycle.hpp"
#include "../../example/simple_stack_allocator.hpp"

typedef boost::context::simple_stack_allocator<
            8 * 1024 * 1024, 64 * 1024, 8 * 1024
        >                                       stack_allocator;

boost::uint64_t jobs = 1000;
ucontext_t uc, ucm;

static void fn()
{ while ( true) ::swapcontext( & uc, & ucm); }

duration_type measure_time()
{
    // cache warum-up
    ::swapcontext( & ucm, & uc);
        
    time_point_type start( clock_type::now() );
    for ( std::size_t i = 0; i < jobs; ++i) {
        ::swapcontext( & ucm, & uc);
    }
    duration_type total = clock_type::now() - start;
    total -= overhead_clock(); // overhead of measurement
    total /= jobs;  // loops
    total /= 2;  // 2x jump_fcontext

    return total;
}

#ifdef BOOST_CONTEXT_CYCLE
cycle_type measure_cycles()
{
    // cache warum-up
    ::swapcontext( & ucm, & uc);
        
    cycle_type start( cycles() );
    for ( std::size_t i = 0; i < jobs; ++i) {
        ::swapcontext( & ucm, & uc);
    }
    cycle_type total = cycles() - start;
    total -= overhead_cycle(); // overhead of measurement
    total /= jobs;  // loops
    total /= 2;  // 2x jump_fcontext

    return total;
}
#endif

int main( int argc, char * argv[])
{
    try
    {
        bind_to_processor( 0);

        boost::program_options::options_description desc("allowed options");
        desc.add_options()
            ("help", "help message")
            ("jobs,j", boost::program_options::value< boost::uint64_t >( & jobs), "jobs to run");

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

        stack_allocator stack_alloc;
        ::getcontext( & uc);
        uc.uc_stack.ss_sp = stack_alloc.allocate( stack_allocator::default_stacksize() );
        uc.uc_stack.ss_size = stack_allocator::default_stacksize();
        ::makecontext( & uc, fn, 7);
 
        boost::uint64_t res = measure_time().count();
        std::cout << "average of " << res << " nano seconds" << std::endl;
#ifdef BOOST_CONTEXT_CYCLE
        res = measure_cycles();
        std::cout << "average of " << res << " cpu cycles" << std::endl;
#endif

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
