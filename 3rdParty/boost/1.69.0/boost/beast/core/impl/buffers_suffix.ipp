//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_SUFFIX_IPP
#define BOOST_BEAST_IMPL_BUFFERS_SUFFIX_IPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/type_traits.hpp>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

template<class Buffers>
class buffers_suffix<Buffers>::const_iterator
{
    friend class buffers_suffix<Buffers>;

    using iter_type = typename
        detail::buffer_sequence_iterator<Buffers>::type;

    iter_type it_;
    buffers_suffix const* b_ = nullptr;

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
                other.it_ == boost::asio::buffer_sequence_end(other.b_->bs_)
            ):(
                (other.b_ == nullptr) ?
                (
                    it_ == boost::asio::buffer_sequence_end(b_->bs_)
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
        return it_ == b_->begin_
            ? value_type{*it_} + b_->skip_
            : *it_;
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
    const_iterator(buffers_suffix const& b,
            iter_type it)
        : it_(it)
        , b_(&b)
    {
    }
};

//------------------------------------------------------------------------------

template<class Buffers>
buffers_suffix<Buffers>::
buffers_suffix()
    : begin_(boost::asio::buffer_sequence_begin(bs_))
{
}

template<class Buffers>
buffers_suffix<Buffers>::
buffers_suffix(buffers_suffix&& other)
    : buffers_suffix(std::move(other),
        std::distance<iter_type>(
            boost::asio::buffer_sequence_begin(
                other.bs_), other.begin_))
{
}

template<class Buffers>
buffers_suffix<Buffers>::
buffers_suffix(buffers_suffix const& other)
    : buffers_suffix(other,
        std::distance<iter_type>(
            boost::asio::buffer_sequence_begin(
                other.bs_), other.begin_))
{
}

template<class Buffers>
buffers_suffix<Buffers>::
buffers_suffix(Buffers const& bs)
    : bs_(bs)
    , begin_(boost::asio::buffer_sequence_begin(bs_))
{
    static_assert(
        boost::asio::is_const_buffer_sequence<Buffers>::value||
        boost::asio::is_mutable_buffer_sequence<Buffers>::value,
            "BufferSequence requirements not met");
}

template<class Buffers>
template<class... Args>
buffers_suffix<Buffers>::
buffers_suffix(boost::in_place_init_t, Args&&... args)
    : bs_(std::forward<Args>(args)...)
    , begin_(boost::asio::buffer_sequence_begin(bs_))
{
    static_assert(sizeof...(Args) > 0,
        "Missing constructor arguments");
    static_assert(
        std::is_constructible<Buffers, Args...>::value,
            "Buffers not constructible from arguments");
}

template<class Buffers>
auto
buffers_suffix<Buffers>::
operator=(buffers_suffix&& other) ->
    buffers_suffix&
{
    auto const dist = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.begin_);
    bs_ = std::move(other.bs_);
    begin_ = std::next(
        boost::asio::buffer_sequence_begin(bs_),
            dist);
    skip_ = other.skip_;
    return *this;
}

template<class Buffers>
auto
buffers_suffix<Buffers>::
operator=(buffers_suffix const& other) ->
    buffers_suffix&
{
    auto const dist = std::distance<iter_type>(
        boost::asio::buffer_sequence_begin(other.bs_),
            other.begin_);
    bs_ = other.bs_;
    begin_ = std::next(
        boost::asio::buffer_sequence_begin(bs_), dist);
    skip_ = other.skip_;
    return *this;
}

template<class Buffers>
inline
auto
buffers_suffix<Buffers>::
begin() const ->
    const_iterator
{
    return const_iterator{*this, begin_};
}

template<class Buffers>
inline
auto
buffers_suffix<Buffers>::
end() const ->
    const_iterator
{
    return const_iterator{*this,
        boost::asio::buffer_sequence_end(bs_)};
}

template<class Buffers>
void
buffers_suffix<Buffers>::
consume(std::size_t amount)
{
    using boost::asio::buffer_size;
    auto const end =
        boost::asio::buffer_sequence_end(bs_);
    for(;amount > 0 && begin_ != end; ++begin_)
    {
        auto const len =
            buffer_size(*begin_) - skip_;
        if(amount < len)
        {
            skip_ += amount;
            break;
        }
        amount -= len;
        skip_ = 0;
    }
}

} // beast
} // boost

#endif
