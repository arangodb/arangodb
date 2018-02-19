
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/pin_thread.hpp"

extern "C" {
#include <errno.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
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
    if ( BOOST_UNLIKELY( -1 == ::processor_bind( P_LWPID,
                                 P_MYID,
                                 static_cast< processorid_t >( cpuid),
                                 0) ) ) {
        throw std::system_error(
                std::error_code( errno, std::system_category() ),
                "processor_bind() failed");
    }
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
