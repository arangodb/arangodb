
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/pin_thread.hpp"

extern "C" {
#include <pthread.h>
#include <sched.h>
}

#include <system_error>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace numa {

BOOST_FIBERS_DECL
void pin_thread( std::uint32_t cpuid) {
    cpu_set_t set;
    CPU_ZERO( & set);
    CPU_SET( cpuid, & set);
    int err = 0;
    if ( BOOST_UNLIKELY( 0 != ( err = ::pthread_setaffinity_np( ::pthread_self(), sizeof( set), & set) ) ) ) {
        throw std::system_error(
                std::error_code( err, std::system_category() ),
                "pthread_setaffinity_np() failed");
    }
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
