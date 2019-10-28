//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_ADAPTOR_HPP
#define BOOST_BEAST_IMPL_BUFFERS_ADAPTOR_HPP

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/config/workaround.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
# pragma warning (push)
# pragma warning (disable: 4521) // multiple copy constructors specified
# pragma warning (disable: 4522) // multiple assignment operators specified
#endif

template<class MutableBufferSequence>
template<bool isMutable>
class buffers_adaptor<MutableBufferSequence>::
    readable_bytes
{
    buffers_adaptor const* b_;

public:
    using value_type = typename
        std::conditional<isMutable,
            net::mutable_buffer,
            net::const_buffer>::type;

    class const_iterator;

    readable_bytes() = delete;

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
    readable_bytes(
        readable_bytes const& other)
        : b_(other.b_)
    {
    }

    readable_bytes& operator=(
        readable_bytes const& other)
    {
        b_ = other.b_;
        return *this;
    }
#else
    readable_bytes(
        readable_bytes const&) = default;
    readable_bytes& operator=(
        readable_bytes const&) = default;
#endif

    template<bool isMutable_ = isMutable, class =
        typename std::enable_if<! isMutable_>::type>
    readable_bytes(
        readable_bytes<true> const& other) noexcept
        : b_(other.b_)
    {
    }

    template<bool isMutable_ = isMutable, class =
        typename std::enable_if<! isMutable_>::type>
    readable_bytes& operator=(
        readable_bytes<true> const& other) noexcept
    {
        b_ = other.b_;
        return *this;
    }

    const_iterator
    begin() const;

    const_iterator
    end() const;

private:
    friend class buffers_adaptor;

    readable_bytes(buffers_adaptor const& b)
        : b_(&b)
    {
    }
};

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
# pragma warning (pop)
#endif

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
template<bool isMutable>
class buffers_adaptor<MutableBufferSequence>::
      readable_bytes<isMutable>::
      const_iterator
{
    iter_type it_{};
    buffers_adaptor const* b_ = nullptr;

public:
    using value_type = typename
        std::conditional<isMutable,
            net::mutable_buffer,
            net::const_buffer>::type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const
    {
        return b_ == other.b_ && it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        value_type const b = *it_;
        return value_type{b.data(),
            (b_->out_ == net::buffer_sequence_end(b_->bs_) ||
                it_ != b_->out_) ? b.size() : b_->out_pos_} +
                    (it_ == b_->begin_ ? b_->in_pos_ : 0);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++()
    {
        ++it_;
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    const_iterator&
    operator--()
    {
        --it_;
        return *this;
    }

    const_iterator
    operator--(int)
    {
        auto temp = *this;
        --(*this);
        return temp;
    }

private:
    friend class readable_bytes;

    const_iterator(
        buffers_adaptor const& b,
        iter_type iter)
        : it_(iter)
        , b_(&b)
    {
    }
};

template<class MutableBufferSequence>
template<bool isMutable>
auto
buffers_adaptor<MutableBufferSequence>::
readable_bytes<isMutable>::
begin() const ->
    const_iterator
{
    return const_iterator{*b_, b_->begin_};
}

template<class MutableBufferSequence>
template<bool isMutable>
auto
buffers_adaptor<MutableBufferSequence>::
readable_bytes<isMutable>::
readable_bytes::end() const ->
    const_iterator
{
    return const_iterator{*b_, b_->end_impl()};
}

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
class buffers_adaptor<MutableBufferSequence>::
mutable_buffers_type
{
    buffers_adaptor const* b_;

public:
    using value_type = net::mutable_buffer;

    class const_iterator;

    mutable_buffers_type() = delete;
    mutable_buffers_type(
        mutable_buffers_type const&) = default;
    mutable_buffers_type& operator=(
        mutable_buffers_type const&) = default;

    const_iterator
    begin() const;

    const_iterator
    end() const;

private:
    friend class buffers_adaptor;

    mutable_buffers_type(
            buffers_adaptor const& b)
        : b_(&b)
    {
    }
};

template<class MutableBufferSequence>
class buffers_adaptor<MutableBufferSequence>::
mutable_buffers_type::const_iterator
{
    iter_type it_{};
    buffers_adaptor const* b_ = nullptr;

public:
    using value_type = net::mutable_buffer;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const
    {
        return b_ == other.b_ && it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        value_type const b = *it_;
        return value_type{b.data(),
            it_ == std::prev(b_->end_) ?
                b_->out_end_ : b.size()} +
                    (it_ == b_->out_ ? b_->out_pos_ : 0);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++()
    {
        ++it_;
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    const_iterator&
    operator--()
    {
        --it_;
        return *this;
    }

    const_iterator
    operator--(int)
    {
        auto temp = *this;
        --(*this);
        return temp;
    }

private:
    friend class mutable_buffers_type;

    const_iterator(buffers_adaptor const& b,
            iter_type iter)
        : it_(iter)
        , b_(&b)
    {
    }
};

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
mutable_buffers_type::
begin() const ->
    const_iterator
{
    return const_iterator{*b_, b_->out_};
}

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
mutable_buffers_type::
end() const ->
    const_iterator
{
    return const_iterator{*b_, b_->end_};
}

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
end_impl() const ->
    iter_type
{
    return out_ == end_ ? end_ : std::next(out_);
}

template<class MutableBufferSequence>
buffers_adaptor<MutableBufferSequence>::
buffers_adaptor(
    buffers_adaptor const& other,
    std::size_t nbegin,
    std::size_t nout,
    std::size_t nend)
    : bs_(other.bs_)
    , begin_(std::next(bs_.begin(), nbegin))
    , out_(std::next(bs_.begin(), nout))
    , end_(std::next(bs_.begin(), nend))
    , max_size_(other.max_size_)
    , in_pos_(other.in_pos_)
    , in_size_(other.in_size_)
    , out_pos_(other.out_pos_)
    , out_end_(other.out_end_)
{
}

template<class MutableBufferSequence>
buffers_adaptor<MutableBufferSequence>::
buffers_adaptor(MutableBufferSequence const& bs)
    : bs_(bs)
    , begin_(net::buffer_sequence_begin(bs_))
    , out_  (net::buffer_sequence_begin(bs_))
    , end_  (net::buffer_sequence_begin(bs_))
    , max_size_(
        [&bs]
        {
            return buffer_bytes(bs);
        }())
{
}

template<class MutableBufferSequence>
template<class... Args>
buffers_adaptor<MutableBufferSequence>::
buffers_adaptor(
    boost::in_place_init_t, Args&&... args)
    : bs_{std::forward<Args>(args)...}
    , begin_(net::buffer_sequence_begin(bs_))
    , out_  (net::buffer_sequence_begin(bs_))
    , end_  (net::buffer_sequence_begin(bs_))
    , max_size_(
        [&]
        {
            return buffer_bytes(bs_);
        }())
{
}

template<class MutableBufferSequence>
buffers_adaptor<MutableBufferSequence>::
buffers_adaptor(buffers_adaptor const& other)
    : buffers_adaptor(
        other,
        std::distance<iter_type>(
            net::buffer_sequence_begin(other.bs_),
            other.begin_),
        std::distance<iter_type>(
            net::buffer_sequence_begin(other.bs_),
            other.out_),
        std::distance<iter_type>(
            net::buffer_sequence_begin(other.bs_),
            other.end_))
{
}

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
operator=(buffers_adaptor const& other) ->
    buffers_adaptor&
{
    if(this == &other)
        return *this;
    auto const nbegin = std::distance<iter_type>(
        net::buffer_sequence_begin(other.bs_),
            other.begin_);
    auto const nout = std::distance<iter_type>(
        net::buffer_sequence_begin(other.bs_),
            other.out_);
    auto const nend = std::distance<iter_type>(
        net::buffer_sequence_begin(other.bs_),
            other.end_);
    bs_ = other.bs_;
    begin_ = std::next(
        net::buffer_sequence_begin(bs_), nbegin);
    out_ =   std::next(
        net::buffer_sequence_begin(bs_), nout);
    end_ =   std::next(
        net::buffer_sequence_begin(bs_), nend);
    max_size_ = other.max_size_;
    in_pos_ = other.in_pos_;
    in_size_ = other.in_size_;
    out_pos_ = other.out_pos_;
    out_end_ = other.out_end_;
    return *this;
}

//

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
data() const noexcept ->
    const_buffers_type
{
    return const_buffers_type{*this};
}

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
data() noexcept ->
    mutable_data_type
{
    return mutable_data_type{*this};
}

template<class MutableBufferSequence>
auto
buffers_adaptor<MutableBufferSequence>::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    end_ = out_;
    if(end_ != net::buffer_sequence_end(bs_))
    {
        auto size = buffer_bytes(*end_) - out_pos_;
        if(n > size)
        {
            n -= size;
            while(++end_ !=
                net::buffer_sequence_end(bs_))
            {
                size = buffer_bytes(*end_);
                if(n < size)
                {
                    out_end_ = n;
                    n = 0;
                    ++end_;
                    break;
                }
                n -= size;
                out_end_ = size;
            }
        }
        else
        {
            ++end_;
            out_end_ = out_pos_ + n;
            n = 0;
        }
    }
    if(n > 0)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffers_adaptor too long"});
    return mutable_buffers_type{*this};
}

template<class MutableBufferSequence>
void
buffers_adaptor<MutableBufferSequence>::
commit(std::size_t n) noexcept
{
    if(out_ == end_)
        return;
    auto const last = std::prev(end_);
    while(out_ != last)
    {
        auto const avail =
            buffer_bytes(*out_) - out_pos_;
        if(n < avail)
        {
            out_pos_ += n;
            in_size_ += n;
            return;
        }
        ++out_;
        n -= avail;
        out_pos_ = 0;
        in_size_ += avail;
    }

    n = std::min<std::size_t>(
        n, out_end_ - out_pos_);
    out_pos_ += n;
    in_size_ += n;
    if(out_pos_ == buffer_bytes(*out_))
    {
        ++out_;
        out_pos_ = 0;
        out_end_ = 0;
    }
}

template<class MutableBufferSequence>
void
buffers_adaptor<MutableBufferSequence>::
consume(std::size_t n) noexcept
{
    while(begin_ != out_)
    {
        auto const avail =
            buffer_bytes(*begin_) - in_pos_;
        if(n < avail)
        {
            in_size_ -= n;
            in_pos_ += n;
            return;
        }
        n -= avail;
        in_size_ -= avail;
        in_pos_ = 0;
        ++begin_;
    }
    auto const avail = out_pos_ - in_pos_;
    if(n < avail)
    {
        in_size_ -= n;
        in_pos_ += n;
    }
    else
    {
        in_size_ -= avail;
        in_pos_ = out_pos_;
    }
}

} // beast
} // boost

#endif
