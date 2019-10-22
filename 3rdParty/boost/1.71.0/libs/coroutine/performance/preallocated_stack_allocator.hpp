
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBER_PREALLOCATED_STACK_ALLOCATOR_H
#define BOOST_FIBER_PREALLOCATED_STACK_ALLOCATOR_H

#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/coroutine/standard_stack_allocator.hpp>
#include <boost/coroutine/stack_context.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

class preallocated_stack_allocator
{
private:
    boost::coroutines::stack_context    stack_ctx_;

public:
    preallocated_stack_allocator() :
        stack_ctx_()
    {
        boost::coroutines::standard_stack_allocator allocator;
        allocator.allocate(
            stack_ctx_,
            boost::coroutines::standard_stack_allocator::traits_type::default_size() ); 
    }

    void allocate( boost::coroutines::stack_context & ctx, std::size_t size)
    {
        ctx.sp = stack_ctx_.sp;
        ctx.size = stack_ctx_.size;
    }

    void deallocate( boost::coroutines::stack_context & ctx)
    {
    }
};

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBER_PREALLOCATED_STACK_ALLOCATOR_H
