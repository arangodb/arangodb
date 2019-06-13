//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MULTI_BUFFER_HPP
#define BOOST_BEAST_MULTI_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/intrusive/list.hpp>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {

/** A @b DynamicBuffer that uses multiple buffers internally.

    The implementation uses a sequence of one or more character arrays
    of varying sizes. Additional character array objects are appended to
    the sequence to accommodate changes in the size of the character
    sequence.

    @note Meets the requirements of @b DynamicBuffer.

    @tparam Allocator The allocator to use for managing memory.
*/
template<class Allocator>
class basic_multi_buffer
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        typename detail::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
    using base_alloc_type = typename
        detail::allocator_traits<Allocator>::
            template rebind_alloc<char>;

    // Storage for the list of buffers representing the input
    // and output sequences. The allocation for each element
    // contains `element` followed by raw storage bytes.
    class element;

    using alloc_traits = detail::allocator_traits<base_alloc_type>;
    using list_type = typename boost::intrusive::make_list<element,
        boost::intrusive::constant_time_size<true>>::type;
    using iter = typename list_type::iterator;
    using const_iter = typename list_type::const_iterator;

    using size_type = typename alloc_traits::size_type;
    using const_buffer = boost::asio::const_buffer;
    using mutable_buffer = boost::asio::mutable_buffer;

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<iter>::iterator_category>::value,
            "BidirectionalIterator requirements not met");

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<const_iter>::iterator_category>::value,
            "BidirectionalIterator requirements not met");

    std::size_t max_ =
        (std::numeric_limits<std::size_t>::max)();
    list_type list_;        // list of allocated buffers
    iter out_;              // element that contains out_pos_
    size_type in_size_ = 0; // size of the input sequence
    size_type in_pos_ = 0;  // input offset in list_.front()
    size_type out_pos_ = 0; // output offset in *out_
    size_type out_end_ = 0; // output end offset in list_.back()

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

#if BOOST_BEAST_DOXYGEN
    /// The type used to represent the input sequence as a list of buffers.
    using const_buffers_type = implementation_defined;

    /// The type used to represent the output sequence as a list of buffers.
    using mutable_buffers_type = implementation_defined;

#else
    class const_buffers_type;

    class mutable_buffers_type;

#endif

    /// Destructor
    ~basic_multi_buffer();

    /** Constructor

        Upon construction, capacity will be zero.
    */
    basic_multi_buffer();

    /** Constructor.

        @param limit The setting for @ref max_size.
    */
    explicit
    basic_multi_buffer(std::size_t limit);

    /** Constructor.

        @param alloc The allocator to use.
    */
    explicit
    basic_multi_buffer(Allocator const& alloc);

    /** Constructor.

        @param limit The setting for @ref max_size.

        @param alloc The allocator to use.
    */
    basic_multi_buffer(
        std::size_t limit, Allocator const& alloc);

    /** Move constructor

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        The object's state will be as if constructed using
        its current allocator and limit.
    */
    basic_multi_buffer(basic_multi_buffer&& other);

    /** Move constructor

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        The object's state will be as if constructed using
        its current allocator and limit.

        @param alloc The allocator to use.
    */
    basic_multi_buffer(basic_multi_buffer&& other,
        Allocator const& alloc);

    /** Copy constructor.

        @param other The object to copy from.
    */
    basic_multi_buffer(basic_multi_buffer const& other);

    /** Copy constructor

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    basic_multi_buffer(basic_multi_buffer const& other,
        Allocator const& alloc);

    /** Copy constructor.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other);

    /** Copy constructor.

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other, allocator_type const& alloc);

    /** Move assignment

        After the move, `*this` will have an empty output sequence.

        @param other The object to move from. After the move,
        The object's state will be as if constructed using
        its current allocator and limit.
    */
    basic_multi_buffer&
    operator=(basic_multi_buffer&& other);

    /** Copy assignment

        After the copy, `*this` will have an empty output sequence.

        @param other The object to copy from.
    */
    basic_multi_buffer& operator=(basic_multi_buffer const& other);

    /** Copy assignment

        After the copy, `*this` will have an empty output sequence.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_multi_buffer& operator=(
        basic_multi_buffer<OtherAlloc> const& other);

    /// Returns a copy of the associated allocator.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    /// Returns the size of the input sequence.
    size_type
    size() const
    {
        return in_size_;
    }

    /// Returns the permitted maximum sum of the sizes of the input and output sequence.
    size_type
    max_size() const
    {
        return max_;
    }

    /// Returns the maximum sum of the sizes of the input sequence and output sequence the buffer can hold without requiring reallocation.
    std::size_t
    capacity() const;

    /** Get a list of buffers that represents the input sequence.

        @note These buffers remain valid across subsequent calls to `prepare`.
    */
    const_buffers_type
    data() const;

    /** Get a list of buffers that represents the output sequence, with the given size.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    mutable_buffers_type
    prepare(size_type n);

    /** Move bytes from the output sequence to the input sequence.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    void
    commit(size_type n);

    /// Remove bytes from the input sequence.
    void
    consume(size_type n);

    template<class Alloc>
    friend
    void
    swap(
        basic_multi_buffer<Alloc>& lhs,
        basic_multi_buffer<Alloc>& rhs);

private:
    template<class OtherAlloc>
    friend class basic_multi_buffer;

    void
    delete_list();

    void
    reset();

    template<class DynamicBuffer>
    void
    copy_from(DynamicBuffer const& other);

    void
    move_assign(basic_multi_buffer& other, std::false_type);

    void
    move_assign(basic_multi_buffer& other, std::true_type);

    void
    copy_assign(basic_multi_buffer const& other, std::false_type);

    void
    copy_assign(basic_multi_buffer const& other, std::true_type);

    void
    swap(basic_multi_buffer&);

    void
    swap(basic_multi_buffer&, std::true_type);

    void
    swap(basic_multi_buffer&, std::false_type);

    void
    debug_check() const;
};

/// A typical multi buffer
using multi_buffer = basic_multi_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/multi_buffer.ipp>

#endif
