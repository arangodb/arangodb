//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_PREFIX_IPP
#define BOOST_BEAST_IMPL_BUFFERS_PREFIX_IPP

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

namespace detail {

inline
boost::asio::const_buffer
buffers_prefix(std::size_t size,
    boost::asio::const_buffer buffer)
{
    return {buffer.data(),
        (std::min)(size, buffer.size())};
}

inline
boost::asio::mutable_buffer
buffers_prefix(std::size_t size,
    boost::asio::mutable_buffer buffer)
{
    return {buffer.data(),
        (std::min)(size, buffer.size())};
}

} // detail

template<class BufferSequence>
class buffers_prefix_view<BufferSequence>::const_iterator
{
    friend class buffers_prefix_view<BufferSequence>;

    buffers_prefix_view const* b_ = nullptr;
    std::size_t remain_;
    iter_type it_;

public:
    using value_type = typename std::conditional<
        boost::is_convertible<typename
            std::iterator_traits<iter_type>::value_type,
                boost::asio::mutable_buffer>::value,
                    boost::asio::mutable_buffer,
                        boost::asio::const_buffer>::type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const
    {
        return
            (b_ == nullptr) ?
            (
                other.b_ == nullptr ||
                other.it_ == other.b_->end_
            ):(
                (other.b_ == nullptr) ?
                (
                    it_ == b_->end_
                ): (
                    b_ == other.b_ &&
                    it_ == other.it_
                )
            );
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return detail::buffers_prefix(remain_, *it_);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++()
    {
        remain_ -= boost::asio::buffer_size(*it_++);
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        remain_ -= boost::asio::buffer_size(*it_++);
        return temp;
    }

    const_iterator&
    operator--()
    {
        remain_ += boost::asio::buffer_size(*--it_);
        return *this;
    }

    const_iterator
    operator--(int)
    {
        auto temp = *this;
        remain_ += boost::asio::buffer_size(*--it_);
        return temp;
    }

private:
    const_iterator(buffers_prefix_view const& b,
            std::true_type)
        : b_(&b)
        , remain_(b.remain_)
        , it_(b_->end_)
    {
    }

    const_iterator(buffers_prefix_view const& b,
            std::false_type)
        : b_(&b)
        , remain_(b_->size_)
        , it_(boost::asio::buffer_sequence_begin(b_->bs_))
    {
    }
};

//------------------------------------------------------------------------------

template<class BufferSequence>
void
buffers_prefix_view<BufferSequence>::
setup(std::size_t size)
{
    size_ = 0;
    remain_ = 0;
    end_ = boost::asio::buffer_sequence_begin(bs_);
    auto const last = bs_.end();
    while(end_ != last)
    {
        auto const len =
            boost::asio::buffer_size(*end_++);
        if(len >= size)
        {
            size_ += size;
            remain_ = size - len;
            break;
        }
        size -= len;
        size_ += len;
    }
}

template<class BufferSequence>
buffers_prefix_view<BufferSequence>::
buffers_prefix_view(buffers_prefix_view&& other)
    : buffers_prefix_view(std::move(other),
        std::distance<iter_type>(
            boost::asio::buffer_sequence_begin(other.bs_),
            other.end_))
{
}

template<class BufferSequence>
buffers_prefix_view<BufferSequence>::
buffers_prefix_view(buffers_prefix_view const& other)
    : buffers_prefix_view(other,
        std::distance<iter_type>(
            boost::asio::buffer_sequence_begin(other.bs_),
                other.end_))
{
}

template<class BufferSequence>
auto
buffers_prefix_view<BufferSequence>::
operator=(buffers_prefix_view&& other) ->
    buffers_prefix_view&
{
    auto const dist = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
        other.end_);
    bs_ = std::move(other.bs_);
    size_ = other.size_;
    remain_ = other.remain_;
    end_ = std::next(
        boost::asio::buffer_sequence_begin(bs_),
            dist);
    return *this;
}

template<class BufferSequence>
auto
buffers_prefix_view<BufferSequence>::
operator=(buffers_prefix_view const& other) ->
    buffers_prefix_view&
{
    auto const dist = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
        other.end_);
    bs_ = other.bs_;
    size_ = other.size_;
    remain_ = other.remain_;
    end_ = std::next(
        boost::asio::buffer_sequence_begin(bs_),
            dist);
    return *this;
}

template<class BufferSequence>
buffers_prefix_view<BufferSequence>::
buffers_prefix_view(std::size_t size,
        BufferSequence const& bs)
    : bs_(bs)
{
    setup(size);
}

template<class BufferSequence>
template<class... Args>
buffers_prefix_view<BufferSequence>::
buffers_prefix_view(std::size_t size,
        boost::in_place_init_t, Args&&... args)
    : bs_(std::forward<Args>(args)...)
{
    setup(size);
}

template<class BufferSequence>
inline
auto
buffers_prefix_view<BufferSequence>::begin() const ->
    const_iterator
{
    return const_iterator{*this, std::false_type{}};
}

template<class BufferSequence>
inline
auto
buffers_prefix_view<BufferSequence>::end() const ->
    const_iterator
{
    return const_iterator{*this, std::true_type{}};
}

} // beast
} // boost

#endif
