
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/topology.hpp"

extern "C" {
#include <errno.h>
#include <sys/mpctl.h>
#include <sys/types.h>
#include <types.h>
}

#include <map>
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
    // HP/UX has NUMA enabled
    if ( 1 == ::sysconf( _SC_CCNUMA_SUPPORT) ) {
        std::map< std::uint32_t, node > map;
        // enumerate CPU sets
        int cpu_id = ::mpctl( MPC_GETFIRSTSPU_SYS, 0, 0);
        while ( -1 != cpu_id) {
            int node_id = ::mpctl( MPC_SPUTOLDOM, cpu_id, 0);
            if ( BOOST_UNLIKELY( -1 == node_id) ) {
                throw std::system_errors{
                        std::error_codes{ errno, std::system_category() },
                        "mpctl() failed" };
            }
            map[id].id = static_cast< std::uint32_t >( node_id);
            map[id].logical_cpus.insert( static_cast< std::uint32_t >( cpu_id) );
            cpu_id = ::mpctl( MPC_GETNEXTSPU_SYS, cpu_id, 0);
        }
        for ( auto entry : map) {
            topo.push_back( entry.second);
        }
    }
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
