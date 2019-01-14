//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_FLAT_BUFFER_HPP
#define BOOST_BEAST_FLAT_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <limits>
#include <memory>

namespace boost {
namespace beast {

/** A linear dynamic buffer.

    Objects of this type meet the requirements of @b DynamicBuffer
    and offer additional invariants:
    
    @li Buffer sequences returned by @ref data and @ref prepare
    will always be of length one.

    @li A configurable maximum buffer size may be set upon
    construction. Attempts to exceed the buffer size will throw
    `std::length_error`.

    Upon construction, a maximum size for the buffer may be
    specified. If this limit is exceeded, the `std::length_error`
    exception will be thrown.

    @note This class is designed for use with algorithms that
    take dynamic buffers as parameters, and are optimized
    for the case where the input sequence or output sequence
    is stored in a single contiguous buffer.
*/
template<class Allocator>
class basic_flat_buffer
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        typename detail::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
    enum
    {
        min_size = 512
    };

    template<class OtherAlloc>
    friend class basic_flat_buffer;

    using base_alloc_type = typename
        detail::allocator_traits<Allocator>::
            template rebind_alloc<char>;

    using alloc_traits =
        detail::allocator_traits<base_alloc_type>;

    static
    inline
    std::size_t
    dist(char const* first, char const* last)
    {
        return static_cast<std::size_t>(last - first);
    }

    char* begin_;
    char* in_;
    char* out_;
    char* last_;
    char* end_;
    std::size_t max_;

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// The type used to represent the input sequence as a list of buffers.
    using const_buffers_type = boost::asio::const_buffer;

    /// The type used to represent the output sequence as a list of buffers.
    using mutable_buffers_type = boost::asio::mutable_buffer;

    /// Destructor
    ~basic_flat_buffer();

    /** Constructor

        Upon construction, capacity will be zero.
    */
    basic_flat_buffer();

    /** Constructor

        Upon construction, capacity will be zero.

        @param limit The setting for @ref max_size.
    */
    explicit
    basic_flat_buffer(std::size_t limit);

    /** Constructor

        Upon construction, capacity will be zero.

        @param alloc The allocator to construct with.
    */
    explicit
    basic_flat_buffer(Allocator const& alloc);

    /** Constructor

        Upon construction, capacity will be zero.

        @param limit The setting for @ref max_size.

        @param alloc The allocator to use.
    */
    basic_flat_buffer(
        std::size_t limit, Allocator const& alloc);

    /** Constructor

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        The object's state will be as if constructed using
        its current allocator and limit.
    */
    basic_flat_buffer(basic_flat_buffer&& other);

    /** Constructor

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        The object's state will be as if constructed using
        its current allocator and limit.

        @param alloc The allocator to use.
    */
    basic_flat_buffer(
        basic_flat_buffer&& other, Allocator const& alloc);

    /** Constructor

        @param other The object to copy from.
    */
    basic_flat_buffer(basic_flat_buffer const& other);

    /** Constructor

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    basic_flat_buffer(basic_flat_buffer const& other,
        Allocator const& alloc);

    /** Constructor

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const& other);

    /** Constructor

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const& other,
            Allocator const& alloc);

    /** Assignment

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        the object's state will be as if constructed using
        its current allocator and limit.
    */
    basic_flat_buffer&
    operator=(basic_flat_buffer&& other);

    /** Assignment

        After the copy, `*this` will have an empty output sequence.

        @param other The object to copy from.
    */
    basic_flat_buffer&
    operator=(basic_flat_buffer const& other);

    /** Copy assignment

        After the copy, `*this` will have an empty output sequence.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_flat_buffer&
    operator=(basic_flat_buffer<OtherAlloc> const& other);

    /// Returns a copy of the associated allocator.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    /// Returns the size of the input sequence.
    std::size_t
    size() const
    {
        return dist(in_, out_);
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t
    max_size() const
    {
        return max_;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t
    capacity() const
    {
        return dist(begin_, end_);
    }

    /// Get a list of buffers that represent the input sequence.
    const_buffers_type
    data() const
    {
        return {in_, dist(in_, out_)};
    }

    /** Get a list of buffers that represent the output sequence, with the given size.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Move bytes from the output sequence to the input sequence.

        @param n The number of bytes to move. If this is larger than
        the number of bytes in the output sequences, then the entire
        output sequences is moved.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    commit(std::size_t n)
    {
        out_ += (std::min)(n, dist(out_, last_));
    }

    /** Remove bytes from the input sequence.

        If `n` is greater than the number of bytes in the input
        sequence, all bytes in the input sequence are removed.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    consume(std::size_t n);

    /** Reallocate the buffer to fit the input sequence.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    shrink_to_fit();

    /// Exchange two flat buffers
    template<class Alloc>
    friend
    void
    swap(
        basic_flat_buffer<Alloc>& lhs,
        basic_flat_buffer<Alloc>& rhs);

private:
    void
    reset();

    template<class DynamicBuffer>
    void
    copy_from(DynamicBuffer const& other);

    void
    move_assign(basic_flat_buffer&, std::true_type);

    void
    move_assign(basic_flat_buffer&, std::false_type);

    void
    copy_assign(basic_flat_buffer const&, std::true_type);

    void
    copy_assign(basic_flat_buffer const&, std::false_type);

    void
    swap(basic_flat_buffer&);

    void
    swap(basic_flat_buffer&, std::true_type);

    void
    swap(basic_flat_buffer&, std::false_type);
};

using flat_buffer =
    basic_flat_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/flat_buffer.ipp>

#endif
