
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/topology.hpp"

extern "C" {
#include <windows.h>
}

#include <map>
#include <set>
#include <system_error>
#include <vector>

#include <boost/assert.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace {

class procinfo_iterator {
private:
    using SLPI = SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;

    SLPI    *   buffer_{ nullptr };
    SLPI    *   procinfo_{ nullptr };
    DWORD       length_{ 0 };

public:
    procinfo_iterator() = default;

    procinfo_iterator( LOGICAL_PROCESSOR_RELATIONSHIP relship) {
        if ( ::GetLogicalProcessorInformationEx( relship, nullptr, & length_) ) {
            return;
        }
        if ( BOOST_UNLIKELY( ERROR_INSUFFICIENT_BUFFER != ::GetLastError() ) ) {
            throw std::system_error{
                    std::error_code{ static_cast< int >( ::GetLastError() ), std::system_category() },
                    "::GetLogicalProcessorInformation() failed" };
        }
        buffer_ = reinterpret_cast< SLPI * >( LocalAlloc( LMEM_FIXED, length_) );
        if ( BOOST_UNLIKELY( nullptr == buffer_) ) {
            throw std::bad_alloc();
        }
        if ( BOOST_UNLIKELY( ! ::GetLogicalProcessorInformationEx( relship, buffer_, & length_) ) ) {
            throw std::system_error{
                    std::error_code{ static_cast< int >( ::GetLastError() ), std::system_category() },
                    "::GetLogicalProcessorInformation() failed" };
        }
        procinfo_ = buffer_;
    }

    procinfo_iterator & operator++() noexcept {
        if ( nullptr != procinfo_) {
            length_ -= procinfo_->Size;
            if ( 0 != length_) {
                procinfo_ = reinterpret_cast< SLPI * >( reinterpret_cast< BYTE * >( procinfo_) + procinfo_->Size); 
            } else {
                LocalFree( buffer_);
                buffer_ = nullptr;
                procinfo_ = nullptr;
            }
        }
        return * this;
    }

    procinfo_iterator operator++( int) {
        procinfo_iterator tmp( * this);
        operator++();
        return tmp;
    } 

    SLPI * operator->() noexcept {
        return procinfo_;
    }

    bool operator==( procinfo_iterator const& other) const noexcept {
        return other.buffer_ == buffer_ && other.procinfo_ == procinfo_ && other.length_ == length_;
    }

    bool operator!=( procinfo_iterator const& other) const noexcept {
        return ! ( * this == other);
    }
};

std::set< std::uint32_t > compute_cpu_set( WORD group_id, KAFFINITY mask) {
	std::set< std::uint32_t > cpus;
	for ( int i = 0; i < sizeof( mask) * 8; ++i) {
  		if ( mask & ( static_cast< KAFFINITY >( 1) << i) ) {
   			cpus.insert( static_cast< std::uint32_t >( 64 * group_id + i) );
  		}
 	}
	return cpus;
}

}

namespace boost {
namespace fibers {
namespace numa {

std::vector< node > topology() {
    std::vector< node > topo;
    procinfo_iterator e;
    for ( procinfo_iterator i{ RelationNumaNode }; i != e; ++i) {
        node n;
        n.id = static_cast< std::uint32_t >( i->NumaNode.NodeNumber);
        auto cpus = compute_cpu_set( i->NumaNode.GroupMask.Group, i->NumaNode.GroupMask.Mask);
        for ( auto cpu_id : cpus) {
            n.logical_cpus.insert( static_cast< std::uint32_t >( cpu_id) );
        }
        topo.push_back( n);
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
