
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/condition_variable.hpp"

#include "boost/fiber/context.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

void
condition_variable_any::notify_one() noexcept {
    detail::spinlock_lock lk{ wait_queue_splk_ };
    wait_queue_.notify_one();
}

void
condition_variable_any::notify_all() noexcept {
    detail::spinlock_lock lk{ wait_queue_splk_ };
    wait_queue_.notify_all();
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
