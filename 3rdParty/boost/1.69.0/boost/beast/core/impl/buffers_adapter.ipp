//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_ADAPTER_IPP
#define BOOST_BEAST_IMPL_BUFFERS_ADAPTER_IPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace boost {
namespace beast {

template<class MutableBufferSequence>
class buffers_adapter<MutableBufferSequence>::
    const_buffers_type
{
    buffers_adapter const* b_;

public:
    using value_type = boost::asio::const_buffer;

    class const_iterator;

    const_buffers_type() = delete;
    const_buffers_type(
        const_buffers_type const&) = default;
    const_buffers_type& operator=(
        const_buffers_type const&) = default;

    const_iterator
    begin() const;

    const_iterator
    end() const;

private:
    friend class buffers_adapter;

    const_buffers_type(buffers_adapter const& b)
        : b_(&b)
    {
    }
};

template<class MutableBufferSequence>
class buffers_adapter<MutableBufferSequence>::
    const_buffers_type::const_iterator
{
    iter_type it_;
    buffers_adapter const* b_ = nullptr;

public:
    using value_type = boost::asio::const_buffer;
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
                other.it_ == other.b_->end_impl()
            ):(
                (other.b_ == nullptr) ?
                (
                    it_ == b_->end_impl()
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
        value_type const b = *it_;
        return value_type{b.data(),
            (b_->out_ == boost::asio::buffer_sequence_end(b_->bs_) ||
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
    friend class const_buffers_type;

    const_iterator(buffers_adapter const& b,
            iter_type iter)
        : it_(iter)
        , b_(&b)
    {
    }
};

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
const_buffers_type::begin() const ->
    const_iterator
{
    return const_iterator{*b_, b_->begin_};
}

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
const_buffers_type::end() const ->
    const_iterator
{
    return const_iterator{*b_, b_->end_impl()};
}

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
class buffers_adapter<MutableBufferSequence>::
mutable_buffers_type
{
    buffers_adapter const* b_;

public:
    using value_type = boost::asio::mutable_buffer;

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
    friend class buffers_adapter;

    mutable_buffers_type(
            buffers_adapter const& b)
        : b_(&b)
    {
    }
};

template<class MutableBufferSequence>
class buffers_adapter<MutableBufferSequence>::
mutable_buffers_type::const_iterator
{
    iter_type it_;
    buffers_adapter const* b_ = nullptr;

public:
    using value_type = boost::asio::mutable_buffer;
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

    const_iterator(buffers_adapter const& b,
            iter_type iter)
        : it_(iter)
        , b_(&b)
    {
    }
};

template<class MutableBufferSequence>
inline
auto
buffers_adapter<MutableBufferSequence>::
mutable_buffers_type::
begin() const ->
    const_iterator
{
    return const_iterator{*b_, b_->out_};
}

template<class MutableBufferSequence>
inline
auto
buffers_adapter<MutableBufferSequence>::
mutable_buffers_type::
end() const ->
    const_iterator
{
    return const_iterator{*b_, b_->end_};
}

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
end_impl() const ->
    iter_type
{
    return out_ == end_ ? end_ : std::next(out_);
}

template<class MutableBufferSequence>
buffers_adapter<MutableBufferSequence>::
buffers_adapter(buffers_adapter&& other)
    : buffers_adapter(std::move(other),
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.begin_),
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.out_),
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.end_))
{
}

template<class MutableBufferSequence>
buffers_adapter<MutableBufferSequence>::
buffers_adapter(buffers_adapter const& other)
    : buffers_adapter(other,
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.begin_),
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.out_),
        std::distance<iter_type>(boost::asio::buffer_sequence_begin(other.bs_), other.end_))
{
}

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
operator=(buffers_adapter&& other) ->
    buffers_adapter&
{
    auto const nbegin = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.begin_);
    auto const nout = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.out_);
    auto const nend = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.end_);
    bs_ = std::move(other.bs_);
    begin_ = std::next(boost::asio::buffer_sequence_begin(bs_), nbegin);
    out_ =   std::next(boost::asio::buffer_sequence_begin(bs_), nout);
    end_ =   std::next(boost::asio::buffer_sequence_begin(bs_), nend);
    max_size_ = other.max_size_;
    in_pos_ = other.in_pos_;
    in_size_ = other.in_size_;
    out_pos_ = other.out_pos_;
    out_end_ = other.out_end_;
    return *this;
}

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
operator=(buffers_adapter const& other) ->
    buffers_adapter&
{
    auto const nbegin = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.begin_);
    auto const nout = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.out_);
    auto const nend = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.end_);
    bs_ = other.bs_;
    begin_ = std::next(boost::asio::buffer_sequence_begin(bs_), nbegin);
    out_ =   std::next(boost::asio::buffer_sequence_begin(bs_), nout);
    end_ =   std::next(boost::asio::buffer_sequence_begin(bs_), nend);
    max_size_ = other.max_size_;
    in_pos_ = other.in_pos_;
    in_size_ = other.in_size_;
    out_pos_ = other.out_pos_;
    out_end_ = other.out_end_;
    return *this;
}

template<class MutableBufferSequence>
buffers_adapter<MutableBufferSequence>::
buffers_adapter(MutableBufferSequence const& bs)
    : bs_(bs)
    , begin_(boost::asio::buffer_sequence_begin(bs_))
    , out_  (boost::asio::buffer_sequence_begin(bs_))
    , end_  (boost::asio::buffer_sequence_begin(bs_))
    , max_size_(boost::asio::buffer_size(bs_))
{
}

template<class MutableBufferSequence>
template<class... Args>
buffers_adapter<MutableBufferSequence>::
buffers_adapter(boost::in_place_init_t, Args&&... args)
    : bs_{std::forward<Args>(args)...}
    , begin_(boost::asio::buffer_sequence_begin(bs_))
    , out_  (boost::asio::buffer_sequence_begin(bs_))
    , end_  (boost::asio::buffer_sequence_begin(bs_))
    , max_size_(boost::asio::buffer_size(bs_))
{
}

template<class MutableBufferSequence>
auto
buffers_adapter<MutableBufferSequence>::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    using boost::asio::buffer_size;
    end_ = out_;
    if(end_ != boost::asio::buffer_sequence_end(bs_))
    {
        auto size = buffer_size(*end_) - out_pos_;
        if(n > size)
        {
            n -= size;
            while(++end_ !=
                boost::asio::buffer_sequence_end(bs_))
            {
                size = buffer_size(*end_);
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
            "buffer overflow"});
    return mutable_buffers_type{*this};
}

template<class MutableBufferSequence>
void
buffers_adapter<MutableBufferSequence>::
commit(std::size_t n)
{
    using boost::asio::buffer_size;
    if(out_ == end_)
        return;
    auto const last = std::prev(end_);
    while(out_ != last)
    {
        auto const avail =
            buffer_size(*out_) - out_pos_;
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

    n = (std::min)(n, out_end_ - out_pos_);
    out_pos_ += n;
    in_size_ += n;
    if(out_pos_ == buffer_size(*out_))
    {
        ++out_;
        out_pos_ = 0;
        out_end_ = 0;
    }
}

template<class MutableBufferSequence>
inline
auto
buffers_adapter<MutableBufferSequence>::
data() const ->
    const_buffers_type
{
    return const_buffers_type{*this};
}

template<class MutableBufferSequence>
void
buffers_adapter<MutableBufferSequence>::
consume(std::size_t n)
{
    using boost::asio::buffer_size;
    while(begin_ != out_)
    {
        auto const avail =
            buffer_size(*begin_) - in_pos_;
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
