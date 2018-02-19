
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/numa/pin_thread.hpp"

extern "C" {
#include <sys/pthread.h>
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
    pthread_spu_t spu;
    int err = ::pthread_processor_bind_np( PTHREAD_BIND_FORCED_NP,
                                           & spu,
                                           static_cast< pthread_spu_t >( cpuid),
                                           PTHREAD_SELFTID_NP);
    if ( BOOST_UNLIKELY( 0 != err) )
        throw std::system_error(
                std::error_code( err, std::system_category() ),
                "pthread_processor_bind_np() failed");
    }
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
