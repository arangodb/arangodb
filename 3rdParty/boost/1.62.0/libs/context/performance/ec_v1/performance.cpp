
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <boost/context/all.hpp>
#include <boost/cstdint.hpp>
#include <boost/program_options.hpp>

#include "../bind_processor.hpp"
#include "../clock.hpp"
#include "../cycle.hpp"

boost::uint64_t jobs = 1000;

boost::context::execution_context * mctx;

static void foo( void *) {
    while ( true) {
        ( * mctx)();
    }
}

duration_type measure_time() {
    boost::context::execution_context ctx_ =
        boost::context::execution_context::current();
    mctx = & ctx_;
    boost::context::execution_context ctx( foo);
    // cache warum-up
    ctx();
    time_point_type start( clock_type::now() );
    for ( std::size_t i = 0; i < jobs; ++i) {
        ctx();
    }
    duration_type total = clock_type::now() - start;
    total -= overhead_clock(); // overhead of measurement
    total /= jobs;  // loops
    total /= 2;  // 2x jump_fcontext
    return total;
}

#ifdef BOOST_CONTEXT_CYCLE
cycle_type measure_cycles() {
    boost::context::execution_context ctx_ =
        boost::context::execution_context::current();
    mctx = & ctx_;
    boost::context::execution_context ctx( foo);
    // cache warum-up
    ctx();
    cycle_type start( cycles() );
    for ( std::size_t i = 0; i < jobs; ++i) {
        ctx();
    }
    cycle_type total = cycles() - start;
    total -= overhead_cycle(); // overhead of measurement
    total /= jobs;  // loops
    total /= 2;  // 2x jump_fcontext

    return total;
}
#endif

int main( int argc, char * argv[]) {
    try {
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
        boost::uint64_t res = measure_time().count();
        std::cout << "execution_context: average of " << res << " nano seconds" << std::endl;
#ifdef BOOST_CONTEXT_CYCLE
        res = measure_cycles();
        std::cout << "execution_context: average of " << res << " cpu cycles" << std::endl;
#endif
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
    return EXIT_FAILURE;
}
