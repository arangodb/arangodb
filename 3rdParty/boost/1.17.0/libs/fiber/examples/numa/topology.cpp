
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <vector>

#include <boost/assert.hpp>
#include <boost/fiber/numa/topology.hpp>

int main( int argc, char * argv[]) {
    try {
        std::vector< boost::fibers::numa::node > topo = boost::fibers::numa::topology();
        for ( auto n : topo) {
            std::cout << "node: " << n.id << " | ";
            std::cout << "cpus: ";
            for ( auto cpu_id : n.logical_cpus) {
                std::cout << cpu_id << " ";
            }
            std::cout << "| distance: ";
            for ( auto d : n.distance) {
                std::cout << d << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "done" << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& ex) {
        std::cerr << "exception: " << ex.what() << std::endl;
    }
    return EXIT_FAILURE;
}
