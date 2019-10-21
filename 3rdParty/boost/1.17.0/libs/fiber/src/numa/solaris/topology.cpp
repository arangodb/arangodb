
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/topology.hpp"

extern "C" {
#include <errno.h>
#include <sys/lgrp_user.h>
}

#include <system_error>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace {

void explore( std::vector< boost::fibers::numa::node > & topo, lgrp_cookie_t cookie, lgrp_id_t node) {
    int size = ::lgrp_cpus( cookie, node, nullptr, 0, LGRP_CONTENT_HIERARCHY);
    if ( -1 == size) {
        return;
    }
    // is node a NUMA Node?
    if ( 0 < ::lgrp_mem_size( cookie, node, LGRP_MEM_SZ_INSTALLED, LGRP_CONTENT_DIRECT) ) {
        std::vector< processorid_t > cpus;
        cpus.resize( size);
        ::lgrp_cpus( cookie, node, cpus.data(), size, LGRP_CONTENT_HIERARCHY);
        boost::fibers::numa::node n;
        n.id = static_cast< std::uint32_t >( node);
        for ( auto cpu_id : cpus) {
            n.logical_cpus.insert( static_cast< std::uint32_t >( cpu_id) );
        }
        topo.push_back( n);
    }
    size = ::lgrp_children( cookie, node, nullptr, 0); 
    std::vector< lgrp_id_t > nodes;
    nodes.resize( size);
    ::lgrp_children( cookie, node, nodes.data(), size);
    for ( auto node : nodes) {
        explore( topo, cookie, node);
    }
}

}

namespace boost {
namespace fibers {
namespace numa {

BOOST_FIBERS_DECL
std::vector< node > topology() {
    std::vector< node > topo;
    lgrp_cookie_t cookie = ::lgrp_init( LGRP_VIEW_OS);
    if ( BOOST_UNLIKELY( LGRP_COOKIE_NONE == cookie) ) {
        throw std::system_error{
                std::error_code{ errno, std::system_category() },
                "lprp_init() failed" };
    }
    lgrp_id_t root = ::lgrp_root( cookie);
    explore( topo, cookie, root);
    ::lgrp_fini( cookie);
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
