//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HANDLER_PTR_HPP
#define BOOST_BEAST_HANDLER_PTR_HPP

#include <boost/beast/core/detail/allocator.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

/** A smart pointer container with associated completion handler.

    This is a smart pointer that retains unique ownership of an
    object through a pointer. Memory is managed using the allocator
    associated with a completion handler stored in the object. The
    managed object is destroyed and its memory deallocated when one
    of the following occurs:

    @li The function @ref invoke is called.

    @li The function @ref release_handler is called.

    @li The container is destroyed.

    Objects of this type are used in the implementation of composed
    operations with states that are expensive or impossible to move.
    This container manages that non-trivial state on behalf of the
    composed operation.

    @par Thread Safety
    @e Distinct @e objects: Safe.@n
    @e Shared @e objects: Unsafe.

    @tparam T The type of the owned object. Must be noexcept destructible.

    @tparam Handler The type of the completion handler.
*/
template<class T, class Handler>
class handler_ptr
{
    using handler_storage_t = typename detail::aligned_union<1, Handler>::type;

    T* t_ = nullptr;
    handler_storage_t h_;

    void clear();

public:
    static_assert(std::is_nothrow_destructible<T>::value,
        "T must be nothrow destructible");

    /// The type of element stored
    using element_type = T;

    /// The type of handler stored
    using handler_type = Handler;

    /// Default constructor (deleted).
    handler_ptr() = delete;

    /// Copy assignment (deleted).
    handler_ptr& operator=(handler_ptr const&) = delete;

    /// Move assignment (deleted).
    handler_ptr& operator=(handler_ptr &&) = delete;

    /** Destructor

        If `*this` owns an object the object is destroyed and
        the memory deallocated using the allocator associated
        with the handler.
    */
    ~handler_ptr();

    /** Move constructor.

        When this call returns, the moved-from container
        will have no owned object.
    */
    handler_ptr(handler_ptr&& other);

    /// Copy constructor (deleted).
    handler_ptr(handler_ptr const& other) = delete;

    /** Constructor

        This creates a new container with an owned object of
        type `T`. The allocator associated with the handler will
        be used to allocate memory for the owned object. The
        constructor for the owned object will be called with the
        following equivalent signature:

        @code
            T::T(Handler const&, Args&&...)
        @endcode

        @par Exception Safety
        Strong guarantee.

        @param handler The handler to associate with the owned
        object. The argument will be moved if it is an xvalue.

        @param args Optional arguments forwarded to
        the owned object's constructor.
    */
    template<class DeducedHandler, class... Args>
    explicit handler_ptr(DeducedHandler&& handler, Args&&... args);

    /// Return a reference to the handler
    handler_type const&
    handler() const
    {
        return *reinterpret_cast<Handler const*>(&h_);
    }

    /// Return a reference to the handler
    handler_type&
    handler()
    {
        return *reinterpret_cast<Handler*>(&h_);
    }

    /// Return `true` if `*this` owns an object
    bool
    has_value() const noexcept
    {
        return t_ != nullptr;
    }

    /** Return a pointer to the owned object.

        @par Preconditions:
        `has_value() == true`
    */
    T*
    get() const
    {
        BOOST_ASSERT(t_);
        return t_;
    }

    /** Return a reference to the owned object.

        @par Preconditions:
        `has_value() == true`
    */
    T&
    operator*() const
    {
        BOOST_ASSERT(t_);
        return *t_;
    }

    /// Return a pointer to the owned object.
    T*
    operator->() const
    {
        return t_;
    }

    /** Return ownership of the handler

        Before this function returns, the owned object is
        destroyed, satisfying the deallocation-before-invocation
        Asio guarantee.

        @return The released handler.

        @par Preconditions:
        `has_value() == true`

        @par Postconditions:
        `has_value() == false`
    */
    handler_type
    release_handler();

    /** Invoke the handler in the owned object.

        This function invokes the handler in the owned object
        with a forwarded argument list. Before the invocation,
        the owned object is destroyed, satisfying the
        deallocation-before-invocation Asio guarantee.

        @par Preconditions:
        `has_value() == true`

        @par Postconditions:
        `has_value() == false`

        @note Care must be taken when the arguments are themselves
        stored in the owned object. Such arguments must first be
        moved to the stack or elsewhere, and then passed, or else
        undefined behavior will result.
    */
    template<class... Args>
    void
    invoke(Args&&... args);
};

} // beast
} // boost

#include <boost/beast/core/impl/handler_ptr.ipp>

#endif
