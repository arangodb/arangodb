
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/topology.hpp"

extern "C" {
#include <errno.h>
#include <sys/param.h>
#include <sys/cpuset.h>
}

#include <system_error>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace numa {

BOOST_FIBERS_DECL
std::vector< node > topology() {
    std::vector< node > topo;
    cpuset_t mask;
    CPU_ZERO( & mask);
    if ( BOOST_UNLIKELY( 0 != ::cpuset_getaffinity( CPU_LEVEL_WHICH, CPU_WHICH_CPUSET, -1, sizeof( mask), & mask) ) ) {
        throw std::system_error{
                std::error_code{ errno, std::system_category() },
                "::cpuset_getaffinity() failed" };
    }
    node n;
    n.id = 0; // FreeBSD does not support NUMA
	for ( int cpu_id = 0; cpu_id < CPU_SETSIZE; ++cpu_id) {
		if ( CPU_ISSET( cpu_id, & mask) ) {
            n.logical_cpus.insert( static_cast< std::uint32_t >( cpu_id) );
		}
	}
    topo.push_back( n); 
    // fake NUMA distance
    std::size_t size = topo.size();
    for ( auto & n : topo) {
        for ( std::size_t i = 0; i < size; ++i) {
            n.distance.push_back( n.id == i ? 10 : 20);
        }
    }
    return topo;
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
