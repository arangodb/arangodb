//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_FLAT_STATIC_BUFFER_HPP
#define BOOST_BEAST_FLAT_STATIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace boost {
namespace beast {

/** A flat @b DynamicBuffer with a fixed size internal buffer.

    Buffer sequences returned by @ref data and @ref prepare
    will always be of length one.
    Ownership of the underlying storage belongs to the derived class.

    @note Variables are usually declared using the template class
    @ref flat_static_buffer; however, to reduce the number of instantiations
    of template functions receiving static stream buffer arguments in a
    deduced context, the signature of the receiving function should use
    @ref flat_static_buffer_base.

    When used with @ref flat_static_buffer this implements a dynamic
    buffer using no memory allocations.

    @see @ref flat_static_buffer
*/
class flat_static_buffer_base
{
    char* begin_;
    char* in_;
    char* out_;
    char* last_;
    char* end_;

    flat_static_buffer_base(flat_static_buffer_base const& other) = delete;
    flat_static_buffer_base& operator=(flat_static_buffer_base const&) = delete;

public:
    /** The type used to represent the input sequence as a list of buffers.

        This buffer sequence is guaranteed to have length 1.
    */
    using const_buffers_type = boost::asio::const_buffer;

    /** The type used to represent the output sequence as a list of buffers.

        This buffer sequence is guaranteed to have length 1.
    */
    using mutable_buffers_type = boost::asio::mutable_buffer;

    /** Constructor

        This creates a dynamic buffer using the provided storage area.

        @param p A pointer to valid storage of at least `n` bytes.

        @param n The number of valid bytes pointed to by `p`.
    */
    flat_static_buffer_base(void* p, std::size_t n)
    {
        reset_impl(p, n);
    }

    /// Return the size of the input sequence.
    std::size_t
    size() const
    {
        return out_ - in_;
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t
    max_size() const
    {
        return dist(begin_, end_);
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t
    capacity() const
    {
        return max_size();
    }

    /** Get a list of buffers that represent the input sequence.

        @note These buffers remain valid across subsequent calls to `prepare`.
    */
    const_buffers_type
    data() const;

    /// Set the input and output sequences to size 0
    void
    reset();

    /** Get a list of buffers that represent the output sequence, with the given size.

        @throws std::length_error if the size would exceed the limit
        imposed by the underlying mutable buffer sequence.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Move bytes from the output sequence to the input sequence.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    void
    commit(std::size_t n)
    {
        out_ += (std::min<std::size_t>)(n, last_ - out_);
    }

    /// Remove bytes from the input sequence.
    void
    consume(std::size_t n)
    {
        consume_impl(n);
    }

protected:
    /** Constructor

        The buffer will be in an undefined state. It is necessary
        for the derived class to call @ref reset with a pointer
        and size in order to initialize the object.
    */
    flat_static_buffer_base() = default;

    /** Reset the pointed-to buffer.

        This function resets the internal state to the buffer provided.
        All input and output sequences are invalidated. This function
        allows the derived class to construct its members before
        initializing the static buffer.

        @param p A pointer to valid storage of at least `n` bytes.

        @param n The number of valid bytes pointed to by `p`.
    */
    void
    reset(void* p, std::size_t n);

private:
    static
    inline
    std::size_t
    dist(char const* first, char const* last)
    {
        return static_cast<std::size_t>(last - first);
    }

    template<class = void>
    void
    reset_impl();

    template<class = void>
    void
    reset_impl(void* p, std::size_t n);

    template<class = void>
    mutable_buffers_type
    prepare_impl(std::size_t n);

    template<class = void>
    void
    consume_impl(std::size_t n);
};

//------------------------------------------------------------------------------

/** A @b DynamicBuffer with a fixed size internal buffer.

    Buffer sequences returned by @ref data and @ref prepare
    will always be of length one.
    This implements a dynamic buffer using no memory allocations.

    @tparam N The number of bytes in the internal buffer.

    @note To reduce the number of template instantiations when passing
    objects of this type in a deduced context, the signature of the
    receiving function should use @ref flat_static_buffer_base instead.

    @see @ref flat_static_buffer_base
*/
template<std::size_t N>
class flat_static_buffer : public flat_static_buffer_base
{
    char buf_[N];

public:
    /// Constructor
    flat_static_buffer(flat_static_buffer const&);

    /// Constructor
    flat_static_buffer()
        : flat_static_buffer_base(buf_, N)
    {
    }

    /// Assignment
    flat_static_buffer& operator=(flat_static_buffer const&);

    /// Returns the @ref flat_static_buffer_base portion of this object
    flat_static_buffer_base&
    base()
    {
        return *this;
    }

    /// Returns the @ref flat_static_buffer_base portion of this object
    flat_static_buffer_base const&
    base() const
    {
        return *this;
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t constexpr
    max_size() const
    {
        return N;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t constexpr
    capacity() const
    {
        return N;
    }
};

} // beast
} // boost

#include <boost/beast/core/impl/flat_static_buffer.ipp>

#endif
