
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBERS_MUTEX_H
#define BOOST_FIBERS_MUTEX_H

#include <boost/config.hpp>

#include <boost/assert.hpp>

#include <boost/fiber/context.hpp>
#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/detail/spinlock.hpp>
#include <boost/fiber/waker.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace boost {
namespace fibers {

class condition_variable;

class BOOST_FIBERS_DECL mutex {
private:
    friend class condition_variable;

    detail::spinlock            wait_queue_splk_{};
    wait_queue                  wait_queue_{};
    context                 *   owner_{ nullptr };

public:
    mutex() = default;

    ~mutex() {
        BOOST_ASSERT( nullptr == owner_);
        BOOST_ASSERT( wait_queue_.empty() );
    }

    mutex( mutex const&) = delete;
    mutex & operator=( mutex const&) = delete;

    void lock();

    bool try_lock();

    void unlock();
};

}}

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_MUTEX_H
