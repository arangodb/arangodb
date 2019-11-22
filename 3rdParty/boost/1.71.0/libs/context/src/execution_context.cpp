
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/context/detail/config.hpp"

#if ! defined(BOOST_CONTEXT_NO_CXX11)

#include "boost/context/execution_context.hpp"
#include <boost/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace context {

#if !defined(BOOST_NO_CXX11_THREAD_LOCAL)

namespace detail {

ecv1_activation_record::ptr_t &
ecv1_activation_record::current() noexcept {
    thread_local static ptr_t current;
    return current;
}

// zero-initialization
thread_local static std::size_t counter;

// schwarz counter
ecv1_activation_record_initializer::ecv1_activation_record_initializer() noexcept {
    if ( 0 == counter++) {
        ecv1_activation_record::current().reset( new ecv1_activation_record() );
    }
}

ecv1_activation_record_initializer::~ecv1_activation_record_initializer() {
    if ( 0 == --counter) {
        BOOST_ASSERT( ecv1_activation_record::current()->is_main_context() );
        delete ecv1_activation_record::current().detach();
    }
}

}

namespace v1 {

execution_context
execution_context::current() noexcept {
    // initialized the first time control passes; per thread
    thread_local static detail::ecv1_activation_record_initializer initializer;
    return execution_context();
}

}

#endif

}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif

#endif
