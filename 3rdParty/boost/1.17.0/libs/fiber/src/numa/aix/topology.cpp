
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/topology.hpp"

extern "C" {
#include <errno.h>
#include <sys/rset.h>
#include <sys/types.h>
}

#include <system_error>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace {

void explore( int sdl, std::vector< boost::fibers::numa::node > & topo) {
    rsethandle_t rset = ::rs_alloc( RS_PARTITION);
    rsethandle_t rad = ::rs_alloc( RS_EMPTY);
    int maxnodes = ::rs_numrads( rset, sdl, 0);
    if ( BOOST_UNLIKELY( -1 == maxnodes) ) {
        throw std::system_error{
                std::error_code{ errno, std::system_category() },
                "rs_numrads() failed" };
    }
    for ( int node_id = 0; node_id < maxnodes; ++node_id) {
        if ( ::rs_getrad( rset, rad, sdl, node_id, 0) ) {
            continue;
        }
        if ( ! ::rs_getinfo( rad, R_NUMPROCS, 0) ) {
            continue;
        }
        boost::fibers::numa::node n;
        n.id = static_cast< std::uint32_t >( node_id);
        int maxcpus = ::rs_getinfo( rad, R_MAXPROCS, 0);
        for ( int cpu_id = 0; cpu_id < maxcpus; ++cpu_id) {
            if ( ::rs_op( RS_TESTRESOURCE, rad, nullptr, R_PROCS, cpu_id) ) {
                n.logical_cpus.insert( static_cast< std::uint32_t >( cpu_id) );
            }
        }
        topo.push_back( n);
    }
    ::rs_free( rset);
    ::rs_free( rad);
}

}

namespace boost {
namespace fibers {
namespace numa {

BOOST_FIBERS_DECL
std::vector< node > topology() {
    std::vector< node > topo;
    int maxlevel = ::rs_getinfo( nullptr, R_MAXSDL, 0);
    for ( int i = 0; i <= maxlevel; ++i) {
        if ( i == ::rs_getinfo( nullptr, R_MCMSDL, 0) ) {
            explore( i, topo);
            break;
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
